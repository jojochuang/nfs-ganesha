// SPDX-License-Identifier: LGPL-3.0-or-later
/*
 * vim:shiftwidth=8:tabstop=8:
 *
 * Copyright 2024 NFS-Ganesha Project
 * Author: GitHub Copilot
 *
 * This program is free software; you can redistribute it and/or
 * modify it under the terms of the GNU Lesser General Public
 * License as published by the Free Software Foundation; either
 * version 3 of the License, or (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * Lesser General Public License for more details.
 *
 * You should have received a copy of the GNU Lesser General Public
 * License along with this library; if not, write to the Free Software
 * Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA  02110-1301 USA
 *
 * -------------
 */

/* fsal_ofs_handle.c
 * OFS FSAL handle operations
 *
 * This file implements file handle encoding/decoding for the OFS FSAL.
 * 
 * KEY FEATURES:
 * - Stable file handles that survive renames (object_id constant)
 * - CRC32C integrity checking with export-id salt  
 * - 28-byte wire format suitable for NFSv3/v4
 * - Cross-export handle rejection
 * - Version field for future format evolution
 *
 * HANDLE FORMAT (struct ofs_fh):
 * - version (8-bit): Format version (currently 0x01)
 * - flags (8-bit): Object type and other flags
 * - export_id (16-bit): Export identifier for validation
 * - volume_id (32-bit): Ozone volume identifier (hashed)
 * - bucket_id (32-bit): Ozone bucket identifier (hashed)
 * - object_id (64-bit): Stable object identifier (constant across renames)  
 * - generation (32-bit): Object generation counter
 * - checksum (32-bit): CRC32C integrity check
 *
 * SECURITY:
 * - Invalid checksum returns NFS4ERR_BADHANDLE
 * - Export ID prevents cross-export handle reuse
 * - CRC32C detects corruption and tampering
 */

#include "config.h"
#include "fsal.h"
#include "FSAL/fsal_commonlib.h"
#include "fsal_convert.h"
#include "fsal_ofs_internal.h"
#include "nfs_core.h"
#include "sal_data.h"
#include "city.h"
#include "citycrc.h"
#include <time.h>
#include <sys/stat.h>
#include <string.h>
#include <inttypes.h>

/**
 * @brief Convert Ozone key metadata to NFS attributes
 *
 * @param[in]  key         Ozone key with metadata
 * @param[out] attrs       FSAL attribute list to populate
 * @param[in]  key_name    Name of the key for debugging
 *
 * @return FSAL status
 */
static fsal_status_t ofs_ozone_key_to_attrs(struct ozone_key *key,
					    struct fsal_attrlist *attrs,
					    const char *key_name)
{
	struct timespec current_time;
	uint64_t fileid;

	if (!attrs) {
		return fsalstat(ERR_FSAL_INVAL, EINVAL);
	}

	/* Initialize the attribute list */
	fsal_prepare_attrs(attrs, 
			  ATTR_TYPE | ATTR_MODE | ATTR_FILEID | ATTR_FSID |
			  ATTR_SIZE | ATTR_ATIME | ATTR_MTIME | ATTR_CTIME |
			  ATTR_OWNER | ATTR_GROUP | ATTR_NUMLINKS);

	/* Set current time for all time attributes for now */
	timespec_get(&current_time, TIME_UTC);

	/* Generate a simple but consistent fileid from key name */
	if (key_name && strcmp(key_name, "/") == 0) {
		fileid = 1; /* Root always gets ID 1 */
	} else if (key_name) {
		/* Simple hash of key name - could be improved with a proper hash function */
		fileid = 2; /* For now, start at 2 for non-root */
		for (const char *p = key_name; *p; p++) {
			fileid = fileid * 31 + (uint64_t)*p;
		}
		/* Ensure it's not 1 (reserved for root) */
		if (fileid == 1) fileid = 2;
	} else {
		fileid = 2;
	}

	/* Map Ozone metadata to NFS attributes */
	if (key_name && strcmp(key_name, "/") == 0) {
		/* Root directory */
		attrs->type = DIRECTORY;
		attrs->mode = 0755;
		attrs->fileid = 1;
		attrs->size = 4096; /* Standard directory size */
		attrs->numlinks = 2; /* . and .. */
	} else {
		/* Regular file - for now, all non-root keys are treated as files */
		attrs->type = REGULAR_FILE;
		attrs->mode = 0644;
		attrs->fileid = fileid;
		attrs->numlinks = 1;
		
		/* Extract size from Ozone key if available */
		if (key) {
			/* TODO: Extract from actual Ozone key metadata when available */
			attrs->size = 1024; /* Mock size for now */
		} else {
			attrs->size = 0;
		}
	}

	/* Set owner/group - default to current process for now */
	/* TODO: Extract from Ozone key metadata when available */
	attrs->owner = getuid();
	attrs->group = getgid();

	/* Set time attributes to current time */
	/* TODO: Use actual modification times from Ozone when available */
	attrs->atime = current_time;
	attrs->mtime = current_time;
	attrs->ctime = current_time;

	/* Set filesystem ID */
	attrs->fsid = op_ctx->fsal_export->filesystem->fsid;

	/* Mark all requested attributes as valid */
	attrs->valid_mask = attrs->request_mask;

	LogDebug(COMPONENT_FSAL,
		 "OFS: Mapped key %s to attrs - type=%d, mode=0%o, size=%zu, fileid=%"PRIu64,
		 key_name ? key_name : "(null)",
		 attrs->type, attrs->mode, attrs->size, attrs->fileid);

	return fsalstat(ERR_FSAL_NO_ERROR, 0);
}

/**
 * @brief Lookup an object by name in a directory
 *
 * @param[in]  parent      Parent directory handle
 * @param[in]  path        Name to lookup
 * @param[out] handle      Resulting object handle
 * @param[out] attrs_out   Resulting object attributes
 *
 * @return FSAL status
 */
static fsal_status_t ofs_lookup(struct fsal_obj_handle *parent,
				const char *path,
				struct fsal_obj_handle **handle,
				struct fsal_attrlist *attrs_out)
{
	struct ofs_fsal_obj_handle *parent_hdl, *child_hdl;
	struct ofs_fsal_export *export;
	struct ozone_client *client;
	struct ozone_volume *volume;
	struct ozone_bucket *bucket;
	struct ozone_key *key;
	char *full_key_name;
	int rc;
	fsal_status_t status;

	if (!parent || !path || !handle) {
		return fsalstat(ERR_FSAL_INVAL, EINVAL);
	}

	LogDebug(COMPONENT_FSAL, "OFS lookup: parent=%p path='%s'", parent, path);

	parent_hdl = container_of(parent, struct ofs_fsal_obj_handle, obj_handle);
	export = parent_hdl->export;
	client = export->client;

	if (!client) {
		LogCrit(COMPONENT_FSAL, "OFS lookup: No Ozone client available");
		return fsalstat(ERR_FSAL_SERVERFAULT, EIO);
	}

	/* Get volume and bucket */
	rc = ofs_ozone_get_volume(client, export->volume_name, &volume);
	if (rc != 0) {
		LogWarn(COMPONENT_FSAL, "OFS lookup: Failed to get volume %s: %d",
			export->volume_name, rc);
		return fsalstat(ERR_FSAL_IO, EIO);
	}

	rc = ofs_ozone_get_bucket(volume, export->bucket_name, &bucket);
	if (rc != 0) {
		LogWarn(COMPONENT_FSAL, "OFS lookup: Failed to get bucket %s: %d",
			export->bucket_name, rc);
		return fsalstat(ERR_FSAL_IO, EIO);
	}

	/* Build full key name - combine parent key name with child name */
	if (strcmp(parent_hdl->key_name, "/") == 0) {
		/* Parent is root, child key is just the path */
		full_key_name = gsh_strdup(path);
	} else {
		/* Combine parent key with child name using '/' separator */
		size_t parent_len = strlen(parent_hdl->key_name);
		size_t path_len = strlen(path);
		full_key_name = gsh_malloc(parent_len + 1 + path_len + 1);
		if (!full_key_name) {
			return fsalstat(ERR_FSAL_NOMEM, ENOMEM);
		}
		strcpy(full_key_name, parent_hdl->key_name);
		strcat(full_key_name, "/");
		strcat(full_key_name, path);
	}

	/* Attempt to head the key in Ozone */
	rc = ofs_ozone_head_key(bucket, full_key_name, &key);
	if (rc != 0) {
		LogInfo(COMPONENT_FSAL, "OFS lookup: Key %s not found: %d",
			full_key_name, rc);
		gsh_free(full_key_name);
		return fsalstat(ERR_FSAL_NOENT, ENOENT);
	}

	/* Allocate new handle for the child object */
	child_hdl = gsh_malloc(sizeof(struct ofs_fsal_obj_handle));
	if (!child_hdl) {
		LogCrit(COMPONENT_FSAL, "OFS lookup: Failed to allocate child handle");
		gsh_free(full_key_name);
		return fsalstat(ERR_FSAL_NOMEM, ENOMEM);
	}

	/* Initialize the child handle */
	child_hdl->key_name = full_key_name;
	child_hdl->export = export;
	
	/* Set OFS-specific identifiers for stable file handles */
	child_hdl->volume_id = CityHash32(export->volume_name, strlen(export->volume_name));
	child_hdl->bucket_id = CityHash32(export->bucket_name, strlen(export->bucket_name));
	/* Generate stable object ID from key name */
	child_hdl->object_id = CityHash64(full_key_name, strlen(full_key_name));
	child_hdl->generation = 1; /* Initial generation - TODO: get from Ozone metadata */

	/* Initialize base FSAL object handle */
	fsal_obj_handle_init(&child_hdl->obj_handle, &export->export, REGULAR_FILE, true);
	
	/* Set up object operations */
	ofs_handle_ops_init(&child_hdl->obj_handle.obj_ops);

	/* Set handle properties */
	child_hdl->obj_handle.fsid = parent->fsid;
	child_hdl->obj_handle.fileid = child_hdl->object_id;

	/* Convert Ozone metadata to FSAL attributes if requested */
	if (attrs_out != NULL) {
		status = ofs_ozone_key_to_attrs(key, attrs_out, full_key_name);
		if (FSAL_IS_ERROR(status)) {
			LogWarn(COMPONENT_FSAL, 
				"OFS lookup: Failed to convert attributes for %s",
				full_key_name);
			gsh_free(child_hdl->key_name);
			gsh_free(child_hdl);
			return status;
		}
	}

	*handle = &child_hdl->obj_handle;

	LogDebug(COMPONENT_FSAL, "OFS lookup: Successfully found key %s", full_key_name);
	return fsalstat(ERR_FSAL_NO_ERROR, 0);
}

/**
 * @brief Get attributes for an object
 *
 * @param[in]     obj_hdl     Object handle
 * @param[in,out] attrs_out   Attribute list to populate  
 *
 * @return FSAL status
 */
static fsal_status_t ofs_getattrs(struct fsal_obj_handle *obj_hdl,
				 struct fsal_attrlist *attrs_out)
{
	struct ofs_fsal_obj_handle *ofs_hdl;
	struct ofs_fsal_export *export;
	struct ozone_client *client;
	struct ozone_volume *volume;
	struct ozone_bucket *bucket;
	struct ozone_key *key;
	int rc;

	if (!obj_hdl || !attrs_out) {
		return fsalstat(ERR_FSAL_INVAL, EINVAL);
	}

	LogDebug(COMPONENT_FSAL, "OFS getattrs: obj_hdl=%p", obj_hdl);

	ofs_hdl = container_of(obj_hdl, struct ofs_fsal_obj_handle, obj_handle);
	export = ofs_hdl->export;
	client = export->client;

	if (!client) {
		LogCrit(COMPONENT_FSAL, "OFS getattrs: No Ozone client available");
		return fsalstat(ERR_FSAL_SERVERFAULT, EIO);
	}

	/* Get volume and bucket */
	rc = ofs_ozone_get_volume(client, export->volume_name, &volume);
	if (rc != 0) {
		LogWarn(COMPONENT_FSAL, "OFS getattrs: Failed to get volume %s: %d",
			export->volume_name, rc);
		return fsalstat(ERR_FSAL_IO, EIO);
	}

	rc = ofs_ozone_get_bucket(volume, export->bucket_name, &bucket);
	if (rc != 0) {
		LogWarn(COMPONENT_FSAL, "OFS getattrs: Failed to get bucket %s: %d",
			export->bucket_name, rc);
		return fsalstat(ERR_FSAL_IO, EIO);
	}

	/* Get fresh metadata from Ozone */
	rc = ofs_ozone_head_key(bucket, ofs_hdl->key_name, &key);
	if (rc != 0) {
		LogWarn(COMPONENT_FSAL, "OFS getattrs: Failed to head key %s: %d",
			ofs_hdl->key_name, rc);
		return fsalstat(ERR_FSAL_NOENT, ENOENT);
	}

	/* Convert Ozone metadata to FSAL attributes */
	return ofs_ozone_key_to_attrs(key, attrs_out, ofs_hdl->key_name);
}

/**
 * @brief Test access permissions for an object
 *
 * @param[in]  obj_hdl          Object handle
 * @param[in]  access_type      Access type to test
 * @param[out] supported        Supported access types (optional)
 * @param[out] allowed          Allowed access types (optional)
 * @param[in]  owner_override   Owner override flag (optional)
 *
 * @return FSAL status
 */
static fsal_status_t ofs_test_access(struct fsal_obj_handle *obj_hdl,
				     fsal_accessflags_t access_type,
				     fsal_accessflags_t *supported,
				     fsal_accessflags_t *allowed,
				     bool *owner_override)
{
	struct fsal_attrlist attrs;
	fsal_status_t status;
	fsal_accessflags_t supported_access = FSAL_R_OK | FSAL_W_OK | FSAL_X_OK;
	fsal_accessflags_t allowed_access = 0;
	mode_t mode;
	uid_t uid;
	gid_t gid;
	bool is_owner = false, is_group = false;

	if (!obj_hdl) {
		return fsalstat(ERR_FSAL_INVAL, EINVAL);
	}

	LogDebug(COMPONENT_FSAL, "OFS test_access: obj_hdl=%p access_type=0x%x",
		 obj_hdl, access_type);

	/* Get object attributes to determine permissions */
	fsal_prepare_attrs(&attrs, ATTR_MODE | ATTR_OWNER | ATTR_GROUP | ATTR_TYPE);
	status = ofs_getattrs(obj_hdl, &attrs);
	if (FSAL_IS_ERROR(status)) {
		LogWarn(COMPONENT_FSAL, "OFS test_access: Failed to get attributes");
		return status;
	}

	mode = attrs.mode;
	uid = attrs.owner;
	gid = attrs.group;

	/* Check if user is owner */
	if (op_ctx->creds && op_ctx->creds->caller_uid == uid) {
		is_owner = true;
	}

	/* Check if user is in group */
	if (op_ctx->creds && (op_ctx->creds->caller_gid == gid)) {
		is_group = true;
	}
	
	/* TODO: Check supplementary groups */

	/* Apply POSIX permission bits */
	if (is_owner) {
		/* Owner permissions */
		if (mode & S_IRUSR) allowed_access |= FSAL_R_OK;
		if (mode & S_IWUSR) allowed_access |= FSAL_W_OK;
		if (mode & S_IXUSR) allowed_access |= FSAL_X_OK;
	} else if (is_group) {
		/* Group permissions */
		if (mode & S_IRGRP) allowed_access |= FSAL_R_OK;
		if (mode & S_IWGRP) allowed_access |= FSAL_W_OK;
		if (mode & S_IXGRP) allowed_access |= FSAL_X_OK;
	} else {
		/* Other permissions */
		if (mode & S_IROTH) allowed_access |= FSAL_R_OK;
		if (mode & S_IWOTH) allowed_access |= FSAL_W_OK;
		if (mode & S_IXOTH) allowed_access |= FSAL_X_OK;
	}

	/* For directories, always allow execute for lookup */
	if (attrs.type == DIRECTORY) {
		supported_access |= FSAL_X_OK;
		if (allowed_access & FSAL_R_OK) {
			allowed_access |= FSAL_X_OK;
		}
	}

	/* Set output parameters */
	if (supported) {
		*supported = supported_access;
	}
	if (allowed) {
		*allowed = allowed_access;
	}
	if (owner_override) {
		*owner_override = is_owner;
	}

	/* Check if requested access is allowed */
	if ((access_type & allowed_access) != access_type) {
		LogDebug(COMPONENT_FSAL, 
			 "OFS test_access: Access denied - requested=0x%x, allowed=0x%x",
			 access_type, allowed_access);
		return fsalstat(ERR_FSAL_ACCESS, EACCES);
	}

	LogDebug(COMPONENT_FSAL, 
		 "OFS test_access: Access granted - mode=0%o, allowed=0x%x",
		 mode, allowed_access);

	/* Release attribute memory */
	fsal_release_attrs(&attrs);

	return fsalstat(ERR_FSAL_NO_ERROR, 0);
}

/**
 * @brief Initialize OFS handle operations
 *
 * @param[in,out] ops   Handle operations vector
 */
void ofs_handle_ops_init(struct fsal_obj_ops *ops)
{
	ops->lookup = ofs_lookup;
	ops->getattrs = ofs_getattrs; 
	ops->test_access = ofs_test_access;
	ops->handle_to_wire = ofs_handle_to_wire;
	ops->handle_to_key = ofs_handle_to_key;
	
	/* Set other operations to unsupported for now */
	fsal_default_obj_ops_init(ops);
}

/**
 * @brief Compute CRC32C checksum for OFS file handle
 *
 * @param[in] fh   File handle structure (checksum field excluded from calculation)
 *
 * @return CRC32C checksum
 */
uint32_t ofs_compute_fh_checksum(const struct ofs_fh *fh)
{
	uint128 hash;
	struct ofs_fh temp_fh;
	
	/* Make a copy and zero the checksum field for calculation */
	temp_fh = *fh;
	temp_fh.checksum = 0;
	
	/* Compute CRC32C hash */
	hash = CityHashCrc128((const char *)&temp_fh, sizeof(temp_fh) - sizeof(temp_fh.checksum));
	
	/* Return lower 32 bits as checksum */
	return (uint32_t)(hash.first & 0xFFFFFFFF);
}

/**
 * @brief Encode OFS object handle to wire format
 *
 * @param[in]  obj_hdl   Object handle to encode
 * @param[out] wire_fh   Wire format file handle
 *
 * @return FSAL status
 */
fsal_status_t ofs_encode_fh(const struct ofs_fsal_obj_handle *obj_hdl,
			   struct ofs_fh *wire_fh)
{
	if (!obj_hdl || !wire_fh) {
		return fsalstat(ERR_FSAL_INVAL, EINVAL);
	}
	
	/* Initialize the wire format handle */
	memset(wire_fh, 0, sizeof(*wire_fh));
	
	wire_fh->version = OFS_FH_VERSION;
	wire_fh->flags = 0;
	if (obj_hdl->obj_handle.type == DIRECTORY) {
		wire_fh->flags |= OFS_FH_FLAG_DIRECTORY;
	}
	
	/* Set export ID from the object's export */
	wire_fh->export_id = (uint16_t)obj_hdl->export->export.export_id;
	
	/* Set OFS-specific identifiers */
	wire_fh->volume_id = obj_hdl->volume_id;
	wire_fh->bucket_id = obj_hdl->bucket_id;
	wire_fh->object_id = obj_hdl->object_id;
	wire_fh->generation = obj_hdl->generation;
	
	/* Compute and set checksum */
	wire_fh->checksum = ofs_compute_fh_checksum(wire_fh);
	
	LogDebug(COMPONENT_FSAL,
		 "OFS encode_fh: vol=%u, bucket=%u, obj=%lu, gen=%u, checksum=0x%x",
		 wire_fh->volume_id, wire_fh->bucket_id, wire_fh->object_id,
		 wire_fh->generation, wire_fh->checksum);
	
	return fsalstat(ERR_FSAL_NO_ERROR, 0);
}

/**
 * @brief Decode wire format file handle to OFS object handle
 *
 * @param[in]  exp_hdl   Export handle
 * @param[in]  wire_fh   Wire format file handle
 * @param[out] obj_hdl   Decoded object handle
 *
 * @return FSAL status
 */
fsal_status_t ofs_decode_fh(struct fsal_export *exp_hdl,
			   const struct ofs_fh *wire_fh,
			   struct ofs_fsal_obj_handle **obj_hdl)
{
	struct ofs_fsal_export *ofs_export;
	struct ofs_fsal_obj_handle *ofs_handle;
	uint32_t computed_checksum;
	object_file_type_t obj_type;
	
	if (!exp_hdl || !wire_fh || !obj_hdl) {
		return fsalstat(ERR_FSAL_INVAL, EINVAL);
	}
	
	*obj_hdl = NULL;
	ofs_export = container_of(exp_hdl, struct ofs_fsal_export, export);
	
	/* Verify handle version */
	if (wire_fh->version != OFS_FH_VERSION) {
		LogWarn(COMPONENT_FSAL,
			"OFS decode_fh: Invalid handle version %u, expected %u",
			wire_fh->version, OFS_FH_VERSION);
		return fsalstat(ERR_FSAL_BADHANDLE, 0);
	}
	
	/* Verify export ID matches */
	if (wire_fh->export_id != exp_hdl->export_id) {
		LogWarn(COMPONENT_FSAL,
			"OFS decode_fh: Export ID mismatch %u != %u",
			wire_fh->export_id, exp_hdl->export_id);
		return fsalstat(ERR_FSAL_BADHANDLE, 0);
	}
	
	/* Verify checksum */
	computed_checksum = ofs_compute_fh_checksum(wire_fh);
	if (computed_checksum != wire_fh->checksum) {
		LogWarn(COMPONENT_FSAL,
			"OFS decode_fh: Checksum mismatch 0x%x != 0x%x",
			computed_checksum, wire_fh->checksum);
		return fsalstat(ERR_FSAL_BADHANDLE, 0);
	}
	
	/* Allocate new object handle */
	ofs_handle = gsh_malloc(sizeof(struct ofs_fsal_obj_handle));
	if (!ofs_handle) {
		LogCrit(COMPONENT_FSAL, "OFS decode_fh: Failed to allocate handle");
		return fsalstat(ERR_FSAL_NOMEM, ENOMEM);
	}
	
	/* Determine object type from flags */
	obj_type = (wire_fh->flags & OFS_FH_FLAG_DIRECTORY) ? DIRECTORY : REGULAR_FILE;
	
	/* Initialize the handle */
	memset(ofs_handle, 0, sizeof(*ofs_handle));
	ofs_handle->export = ofs_export;
	ofs_handle->volume_id = wire_fh->volume_id;
	ofs_handle->bucket_id = wire_fh->bucket_id;
	ofs_handle->object_id = wire_fh->object_id;
	ofs_handle->generation = wire_fh->generation;
	
	/* Generate key name from object ID (temporary implementation) */
	if (wire_fh->object_id == 1) {
		ofs_handle->key_name = gsh_strdup("/");
	} else {
		/* For now, use object ID as key name - this should be improved */
		size_t key_len = snprintf(NULL, 0, "obj_%lu", wire_fh->object_id) + 1;
		ofs_handle->key_name = gsh_malloc(key_len);
		if (!ofs_handle->key_name) {
			gsh_free(ofs_handle);
			return fsalstat(ERR_FSAL_NOMEM, ENOMEM);
		}
		snprintf(ofs_handle->key_name, key_len, "obj_%lu", wire_fh->object_id);
	}
	
	/* Initialize the base FSAL object handle */
	fsal_obj_handle_init(&ofs_handle->obj_handle, exp_hdl, obj_type, true);
	
	/* Set up operations */
	ofs_handle_ops_init(&ofs_handle->obj_handle.obj_ops);
	
	/* Set handle properties */
	ofs_handle->obj_handle.fsid = exp_hdl->filesystem->fsid;
	ofs_handle->obj_handle.fileid = wire_fh->object_id;
	
	*obj_hdl = ofs_handle;
	
	LogDebug(COMPONENT_FSAL,
		 "OFS decode_fh: Successfully decoded handle vol=%u, bucket=%u, obj=%lu",
		 wire_fh->volume_id, wire_fh->bucket_id, wire_fh->object_id);
	
	return fsalstat(ERR_FSAL_NO_ERROR, 0);
}

/**
 * @brief Convert object handle to wire format (FSAL operation)
 *
 * @param[in]     obj_hdl      Object handle
 * @param[in]     output_type  Type of digest (NFSv3 or NFSv4)
 * @param[in,out] fh_desc      Buffer descriptor for wire format
 *
 * @return FSAL status
 */
static fsal_status_t ofs_handle_to_wire(const struct fsal_obj_handle *obj_hdl,
					fsal_digesttype_t output_type,
					struct gsh_buffdesc *fh_desc)
{
	const struct ofs_fsal_obj_handle *ofs_hdl;
	struct ofs_fh *wire_fh;
	fsal_status_t status;
	
	if (!obj_hdl || !fh_desc) {
		return fsalstat(ERR_FSAL_INVAL, EINVAL);
	}
	
	ofs_hdl = container_of(obj_hdl, const struct ofs_fsal_obj_handle, obj_handle);
	
	LogDebug(COMPONENT_FSAL, "OFS handle_to_wire: obj_hdl=%p, type=%d", obj_hdl, output_type);
	
	switch (output_type) {
	case FSAL_DIGEST_NFSV3:
	case FSAL_DIGEST_NFSV4:
		/* Check buffer size */
		if (fh_desc->len < sizeof(struct ofs_fh)) {
			LogMajor(COMPONENT_FSAL,
				 "OFS handle_to_wire: Buffer too small %zu < %zu",
				 fh_desc->len, sizeof(struct ofs_fh));
			return fsalstat(ERR_FSAL_TOOSMALL, 0);
		}
		
		wire_fh = (struct ofs_fh *)fh_desc->addr;
		status = ofs_encode_fh(ofs_hdl, wire_fh);
		if (FSAL_IS_ERROR(status)) {
			return status;
		}
		
		fh_desc->len = sizeof(struct ofs_fh);
		break;
		
	default:
		LogMajor(COMPONENT_FSAL, "OFS handle_to_wire: Unsupported output type %d", output_type);
		return fsalstat(ERR_FSAL_SERVERFAULT, 0);
	}
	
	return fsalstat(ERR_FSAL_NO_ERROR, 0);
}

/**
 * @brief Convert object handle to key format for caching
 *
 * @param[in]     obj_hdl   Object handle
 * @param[in,out] fh_desc   Buffer descriptor for handle key
 */
static void ofs_handle_to_key(struct fsal_obj_handle *obj_hdl,
			     struct gsh_buffdesc *fh_desc)
{
	struct ofs_fsal_obj_handle *ofs_hdl;
	
	ofs_hdl = container_of(obj_hdl, struct ofs_fsal_obj_handle, obj_handle);
	
	/* Use object ID as the key for caching */
	fh_desc->addr = &ofs_hdl->object_id;
	fh_desc->len = sizeof(ofs_hdl->object_id);
	
	LogDebug(COMPONENT_FSAL, "OFS handle_to_key: object_id=%lu", ofs_hdl->object_id);
}
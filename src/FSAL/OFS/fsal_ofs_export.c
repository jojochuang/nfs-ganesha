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

/* fsal_ofs_export.c
 * OFS FSAL export operations
 */

#include "config.h"
#include "fsal.h"
#include "FSAL/fsal_commonlib.h"
#include "FSAL/fsal_config.h"
#include "config_parsing.h"
#include "fsal_ofs_internal.h"
#include "nfs_exports.h"
#include "export_mgr.h"
#include "city.h"

/**
 * @brief Release an export
 *
 * @param[in] exp_hdl	Export handle to release
 */
static void ofs_release(struct fsal_export *exp_hdl)
{
	struct ofs_fsal_export *myself;

	myself = container_of(exp_hdl, struct ofs_fsal_export, export);

	LogDebug(COMPONENT_FSAL,
		 "Releasing OFS export %" PRIu16 " for %s",
		 exp_hdl->export_id,
		 myself->export_path ? myself->export_path : "NULL");

	/* Clean up Ozone client connection */
	if (myself->client) {
		ofs_ozone_disconnect(myself->client);
		myself->client = NULL;
	}

	fsal_detach_export(exp_hdl->fsal, &exp_hdl->exports);
	free_export_ops(exp_hdl);

	/* Clean up allocated strings */
	if (myself->export_path)
		gsh_free(myself->export_path);
	if (myself->volume_name)
		gsh_free(myself->volume_name);
	if (myself->bucket_name)
		gsh_free(myself->bucket_name);
	
	gsh_free(myself);
}

/**
 * @brief Lookup a path
 *
 * @param[in]  exp_hdl   Export handle
 * @param[in]  path      Path to look up
 * @param[out] handle    Handle for the object
 * @param[out] attrs_out Attributes of the object
 *
 * @return FSAL status
 */
/**
 * @brief Lookup a path in the OFS filesystem using Ozone client API
 *
 * @param[in]  exp_hdl      Export handle
 * @param[in]  path         Path to lookup
 * @param[out] handle       Returned object handle
 * @param[out] attrs_out    Returned attributes
 *
 * @return FSAL status
 */
static fsal_status_t ofs_lookup_path(struct fsal_export *exp_hdl,
				     const char *path,
				     struct fsal_obj_handle **handle,
				     struct fsal_attrlist *attrs_out)
{
	struct ofs_fsal_export *ofs_export;
	struct ofs_fsal_obj_handle *ofs_handle;
	struct ozone_client *client;
	struct ozone_volume *volume;
	struct ozone_bucket *bucket;
	struct ozone_key *key;
	const char *key_name;
	int rc;
	
	*handle = NULL;
	
	if (!exp_hdl || !path) {
		LogCrit(COMPONENT_FSAL, "OFS lookup_path: Invalid arguments");
		return fsalstat(ERR_FSAL_INVAL, EINVAL);
	}
	
	ofs_export = container_of(exp_hdl, struct ofs_fsal_export, export);
	
	LogDebug(COMPONENT_FSAL, "OFS lookup_path: Looking up path %s", path);
	
	/* Verify path matches export path for root lookup */
	if (strcmp(path, ofs_export->export_path) != 0) {
		LogDebug(COMPONENT_FSAL, 
			 "OFS lookup_path: Path %s doesn't match export path %s",
			 path, ofs_export->export_path);
		return fsalstat(ERR_FSAL_NOENT, ENOENT);
	}
	
	/* Use existing Ozone client connection or establish new one */
	client = ofs_export->client;
	if (!client) {
		LogCrit(COMPONENT_FSAL, "OFS lookup_path: No Ozone client connection");
		return fsalstat(ERR_FSAL_IO, EIO);
	}
	
	/* Get the Ozone volume */
	rc = ofs_ozone_get_volume(client, ofs_export->volume_name, &volume);
	if (rc != 0) {
		LogCrit(COMPONENT_FSAL, 
			"OFS lookup_path: Failed to get volume %s: %d", 
			ofs_export->volume_name, rc);
		return fsalstat(ERR_FSAL_IO, EIO);
	}
	
	/* Get the Ozone bucket */
	rc = ofs_ozone_get_bucket(volume, ofs_export->bucket_name, &bucket);
	if (rc != 0) {
		LogCrit(COMPONENT_FSAL,
			"OFS lookup_path: Failed to get bucket %s: %d",
			ofs_export->bucket_name, rc);
		return fsalstat(ERR_FSAL_IO, EIO);
	}
	
	/* For root lookup, use a special root key or empty key name */
	key_name = "/";
	
	/* Check if the key exists using Ozone HEAD operation */
	rc = ofs_ozone_head_key(bucket, key_name, &key);
	if (rc != 0) {
		LogInfo(COMPONENT_FSAL,
			"OFS lookup_path: Key %s not found or not accessible: %d",
			key_name, rc);
		return fsalstat(ERR_FSAL_NOENT, ENOENT);
	}
	
	/* Allocate OFS handle structure */
	ofs_handle = gsh_malloc(sizeof(struct ofs_fsal_obj_handle));
	if (!ofs_handle) {
		LogCrit(COMPONENT_FSAL, "OFS lookup_path: Failed to allocate handle");
		return fsalstat(ERR_FSAL_NOMEM, ENOMEM);
	}
	
	/* Store the key name for future operations */
	ofs_handle->key_name = gsh_strdup(key_name);
	if (!ofs_handle->key_name) {
		gsh_free(ofs_handle);
		LogCrit(COMPONENT_FSAL, "OFS lookup_path: Failed to allocate key name");
		return fsalstat(ERR_FSAL_NOMEM, ENOMEM);
	}
	
	/* Back pointer to export */
	ofs_handle->export = ofs_export;
	
	/* Initialize the base FSAL object handle */
	fsal_obj_handle_init(&ofs_handle->obj_handle, exp_hdl, DIRECTORY, true);
	
	/* Set up OFS-specific operations */
	ofs_handle_ops_init(&ofs_handle->obj_handle.obj_ops);
	
	/* Set basic attributes for root directory */
	ofs_handle->obj_handle.fsid = exp_hdl->filesystem->fsid;
	ofs_handle->obj_handle.fileid = 1; /* Root directory gets ID 1 */
	
	/* Set OFS-specific identifiers for stable file handles */
	/* TODO: Get these from actual Ozone volume/bucket metadata */
	ofs_handle->volume_id = CityHash32(ofs_export->volume_name, strlen(ofs_export->volume_name));
	ofs_handle->bucket_id = CityHash32(ofs_export->bucket_name, strlen(ofs_export->bucket_name));
	ofs_handle->object_id = 1; /* Root always gets object ID 1 */
	ofs_handle->generation = 1; /* Initial generation */
	
	/* Fill in attributes if requested */
	if (attrs_out != NULL) {
		fsal_prepare_attrs(attrs_out, ATTR_TYPE | ATTR_MODE | ATTR_FILEID | ATTR_FSID);
		
		attrs_out->type = DIRECTORY;
		attrs_out->mode = 0755;
		attrs_out->fileid = 1;
		attrs_out->fsid = ofs_handle->obj_handle.fsid;
	}
	
	*handle = &ofs_handle->obj_handle;
	
	LogDebug(COMPONENT_FSAL,
		 "OFS lookup_path: Successfully created handle for %s (key: %s)",
		 path, key_name);
	
	return fsalstat(ERR_FSAL_NO_ERROR, 0);
}

/**
 * @brief Create a handle from wire data
 *
 * @param[in]  exp_hdl    Export handle
 * @param[in]  hdl_desc   Wire handle descriptor
 * @param[out] handle     Handle for the object
 * @param[out] attrs_out  Attributes of the object
 *
 * @return FSAL status
 */
static fsal_status_t ofs_create_handle(struct fsal_export *exp_hdl,
				       struct gsh_buffdesc *hdl_desc,
				       struct fsal_obj_handle **handle,
				       struct fsal_attrlist *attrs_out)
{
	struct ofs_fh *wire_fh;
	struct ofs_fsal_obj_handle *ofs_handle;
	fsal_status_t status;
	
	if (!exp_hdl || !hdl_desc || !handle) {
		LogCrit(COMPONENT_FSAL, "OFS create_handle: Invalid arguments");
		return fsalstat(ERR_FSAL_INVAL, EINVAL);
	}
	
	*handle = NULL;
	
	/* Verify buffer size */
	if (hdl_desc->len < sizeof(struct ofs_fh)) {
		LogMajor(COMPONENT_FSAL,
			 "OFS create_handle: Handle buffer too small %zu < %zu",
			 hdl_desc->len, sizeof(struct ofs_fh));
		return fsalstat(ERR_FSAL_BADHANDLE, 0);
	}
	
	wire_fh = (struct ofs_fh *)hdl_desc->addr;
	
	/* Decode the wire format handle */
	status = ofs_decode_fh(exp_hdl, wire_fh, &ofs_handle);
	if (FSAL_IS_ERROR(status)) {
		LogWarn(COMPONENT_FSAL, "OFS create_handle: Failed to decode handle");
		return status;
	}
	
	/* Fill in attributes if requested */
	if (attrs_out != NULL) {
		status = ofs_getattrs(&ofs_handle->obj_handle, attrs_out);
		if (FSAL_IS_ERROR(status)) {
			LogWarn(COMPONENT_FSAL, "OFS create_handle: Failed to get attributes");
			/* Continue anyway - attributes are optional for create_handle */
		}
	}
	
	*handle = &ofs_handle->obj_handle;
	
	LogDebug(COMPONENT_FSAL, "OFS create_handle: Successfully created handle");
	return fsalstat(ERR_FSAL_NO_ERROR, 0);
}

/**
 * @brief Decode wire format handle (export operation)
 *
 * @param[in]     exp_hdl    Export handle
 * @param[in]     in_type    Input digest type
 * @param[in,out] fh_desc    Handle buffer descriptor
 * @param[in]     flags      Decode flags
 *
 * @return FSAL status
 */
static fsal_status_t ofs_wire_to_host(struct fsal_export *exp_hdl,
				      fsal_digesttype_t in_type,
				      struct gsh_buffdesc *fh_desc,
				      int flags)
{
	struct ofs_fh *wire_fh;
	uint32_t computed_checksum;
	
	if (!exp_hdl || !fh_desc) {
		return fsalstat(ERR_FSAL_INVAL, EINVAL);
	}
	
	LogDebug(COMPONENT_FSAL, "OFS wire_to_host: type=%d, len=%zu", in_type, fh_desc->len);
	
	switch (in_type) {
	case FSAL_DIGEST_NFSV3:
	case FSAL_DIGEST_NFSV4:
		/* Verify buffer size */
		if (fh_desc->len < sizeof(struct ofs_fh)) {
			LogMajor(COMPONENT_FSAL,
				 "OFS wire_to_host: Handle too small %zu < %zu",
				 fh_desc->len, sizeof(struct ofs_fh));
			return fsalstat(ERR_FSAL_BADHANDLE, 0);
		}
		
		wire_fh = (struct ofs_fh *)fh_desc->addr;
		
		/* Verify handle version */
		if (wire_fh->version != OFS_FH_VERSION) {
			LogWarn(COMPONENT_FSAL,
				"OFS wire_to_host: Invalid version %u", wire_fh->version);
			return fsalstat(ERR_FSAL_BADHANDLE, 0);
		}
		
		/* Verify export ID */
		if (wire_fh->export_id != exp_hdl->export_id) {
			LogWarn(COMPONENT_FSAL,
				"OFS wire_to_host: Export ID mismatch %u != %u",
				wire_fh->export_id, exp_hdl->export_id);
			return fsalstat(ERR_FSAL_BADHANDLE, 0);
		}
		
		/* Verify checksum */
		computed_checksum = ofs_compute_fh_checksum(wire_fh);
		if (computed_checksum != wire_fh->checksum) {
			LogWarn(COMPONENT_FSAL,
				"OFS wire_to_host: Checksum mismatch 0x%x != 0x%x",
				computed_checksum, wire_fh->checksum);
			return fsalstat(ERR_FSAL_BADHANDLE, 0);
		}
		
		/* Update length to exact handle size */
		fh_desc->len = sizeof(struct ofs_fh);
		break;
		
	default:
		LogMajor(COMPONENT_FSAL, "OFS wire_to_host: Unsupported type %d", in_type);
		return fsalstat(ERR_FSAL_SERVERFAULT, 0);
	}
	
	LogDebug(COMPONENT_FSAL, "OFS wire_to_host: Successfully validated handle");
	return fsalstat(ERR_FSAL_NO_ERROR, 0);
}

/**
 * @brief Initialize export ops vector
 *
 * @param[in,out] ops   Vector of export operations
 */
void ofs_export_ops_init(struct export_ops *ops)
{
	ops->release = ofs_release;
	ops->lookup_path = ofs_lookup_path;
	ops->create_handle = ofs_create_handle;
	ops->wire_to_host = ofs_wire_to_host;
}
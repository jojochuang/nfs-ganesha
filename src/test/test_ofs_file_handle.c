// SPDX-License-Identifier: LGPL-3.0-or-later
/*
 * vim:shiftwidth=8:tabstop=8:
 *
 * Copyright 2024 NFS-Ganesha Project
 * 
 * Unit tests for OFS FSAL file handle encoding/decoding
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <assert.h>
#include <stdbool.h>
#include <unistd.h>
#include <sys/stat.h>
#include <time.h>
#include <errno.h>
#include <stdint.h>
#include <stddef.h>

/* Mock the gsh_* functions for testing */
char *gsh_strdup(const char *s) {
	return s ? strdup(s) : NULL;
}

void gsh_free(void *ptr) {
	free(ptr);
}

void *gsh_malloc(size_t size) {
	return malloc(size);
}

void *gsh_calloc(size_t nmemb, size_t size) {
	return calloc(nmemb, size);
}

/* Mock log functions */
void LogDebug(int component, const char *format, ...) {
	(void)component; (void)format;
}

void LogInfo(int component, const char *format, ...) {
	(void)component; (void)format;
}

void LogWarn(int component, const char *format, ...) {
	(void)component; (void)format;
}

void LogMajor(int component, const char *format, ...) {
	(void)component; (void)format;
}

void LogCrit(int component, const char *format, ...) {
	(void)component; (void)format;
}

#define COMPONENT_FSAL 1

/* Mock FSAL structures and functions */
typedef enum {
	ERR_FSAL_NO_ERROR = 0,
	ERR_FSAL_INVAL,
	ERR_FSAL_BADHANDLE,
	ERR_FSAL_NOMEM,
	ERR_FSAL_TOOSMALL,
	ERR_FSAL_SERVERFAULT
} fsal_errors_t;

typedef struct {
	fsal_errors_t major;
	int minor;
} fsal_status_t;

static inline fsal_status_t fsalstat(fsal_errors_t major, int minor) {
	fsal_status_t status = {major, minor};
	return status;
}

static inline bool FSAL_IS_ERROR(fsal_status_t status) {
	return status.major != ERR_FSAL_NO_ERROR;
}

typedef enum {
	FSAL_DIGEST_NFSV3,
	FSAL_DIGEST_NFSV4
} fsal_digesttype_t;

struct gsh_buffdesc {
	void *addr;
	size_t len;
};

struct fsal_export {
	uint16_t export_id;
};

/* Minimal CityHash function mocks */
uint32_t CityHash32(const char *s, size_t len) {
	uint32_t hash = 5381;
	for (size_t i = 0; i < len; i++) {
		hash = ((hash << 5) + hash) + s[i];
	}
	return hash;
}

uint64_t CityHash64(const char *s, size_t len) {
	uint64_t hash = 14695981039346656037ULL;
	for (size_t i = 0; i < len; i++) {
		hash ^= s[i];
		hash *= 1099511628211ULL;
	}
	return hash;
}

/* Mock CityHashCrc128 */
typedef struct {
	uint64_t first;
	uint64_t second;
} uint128;

uint128 CityHashCrc128(const char *s, size_t len) {
	uint128 result;
	result.first = CityHash64(s, len);
	result.second = CityHash64(s + len/2, len - len/2);
	return result;
}

/* Include the actual OFS structures - inline them for standalone testing */
struct ofs_fh {
	uint8_t version;	/* Handle format version (0x01) */
	uint8_t flags;		/* Reserved flags */
	uint16_t export_id;	/* Export ID for cross-export verification */
	uint32_t volume_id;	/* Ozone volume identifier */
	uint32_t bucket_id;	/* Ozone bucket identifier */
	uint64_t object_id;	/* Ozone object identifier (stable across renames) */
	uint32_t generation;	/* Generation counter for object versioning */
	uint32_t checksum;	/* CRC32C checksum of above fields */
} __attribute__((__packed__));

#define OFS_FH_VERSION		0x01
#define OFS_FH_FLAG_DIRECTORY	0x01

struct ofs_fsal_export {
	struct fsal_export export;	/* Export this wraps */
	char *export_path;		/* The path for this export */
	char *volume_name;		/* Ozone volume name */
	char *bucket_name;		/* Ozone bucket name */
};

struct ofs_fsal_obj_handle {
	char *key_name;			   /* Ozone key name */
	struct ofs_fsal_export *export;   /* Back pointer to export */
	
	/* OFS-specific identifiers for stable file handles */
	uint32_t volume_id;		   /* Ozone volume identifier */
	uint32_t bucket_id;		   /* Ozone bucket identifier */
	uint64_t object_id;		   /* Stable object identifier */
	uint32_t generation;		   /* Object generation counter */
};

/* Function prototypes */
uint32_t ofs_compute_fh_checksum(const struct ofs_fh *fh);
fsal_status_t ofs_encode_fh(const struct ofs_fsal_obj_handle *obj_hdl,
			   struct ofs_fh *wire_fh);
fsal_status_t ofs_decode_fh(struct fsal_export *exp_hdl,
			   const struct ofs_fh *wire_fh,
			   struct ofs_fsal_obj_handle **obj_hdl);

/* Mock export for testing */
static struct ofs_fsal_export test_export = {
	.export = { .export_id = 12345 },
	.volume_name = "test-volume",
	.bucket_name = "test-bucket"
};

/* Test functions */

/* Implementation of OFS functions for testing */
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
	
	/* Set export ID from the object's export */
	wire_fh->export_id = obj_hdl->export->export.export_id;
	
	/* Set OFS-specific identifiers */
	wire_fh->volume_id = obj_hdl->volume_id;
	wire_fh->bucket_id = obj_hdl->bucket_id;
	wire_fh->object_id = obj_hdl->object_id;
	wire_fh->generation = obj_hdl->generation;
	
	/* Compute and set checksum */
	wire_fh->checksum = ofs_compute_fh_checksum(wire_fh);
	
	return fsalstat(ERR_FSAL_NO_ERROR, 0);
}

fsal_status_t ofs_decode_fh(struct fsal_export *exp_hdl,
			   const struct ofs_fh *wire_fh,
			   struct ofs_fsal_obj_handle **obj_hdl)
{
	struct ofs_fsal_export *ofs_export;
	struct ofs_fsal_obj_handle *ofs_handle;
	uint32_t computed_checksum;
	
	if (!exp_hdl || !wire_fh || !obj_hdl) {
		return fsalstat(ERR_FSAL_INVAL, EINVAL);
	}
	
	*obj_hdl = NULL;
	ofs_export = (struct ofs_fsal_export *)((char*)exp_hdl - offsetof(struct ofs_fsal_export, export));
	
	/* Verify handle version */
	if (wire_fh->version != OFS_FH_VERSION) {
		return fsalstat(ERR_FSAL_BADHANDLE, 0);
	}
	
	/* Verify export ID matches */
	if (wire_fh->export_id != exp_hdl->export_id) {
		return fsalstat(ERR_FSAL_BADHANDLE, 0);
	}
	
	/* Verify checksum */
	computed_checksum = ofs_compute_fh_checksum(wire_fh);
	if (computed_checksum != wire_fh->checksum) {
		return fsalstat(ERR_FSAL_BADHANDLE, 0);
	}
	
	/* Allocate new object handle */
	ofs_handle = gsh_malloc(sizeof(struct ofs_fsal_obj_handle));
	if (!ofs_handle) {
		return fsalstat(ERR_FSAL_NOMEM, ENOMEM);
	}
	
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
		/* For now, use object ID as key name */
		size_t key_len = snprintf(NULL, 0, "obj_%lu", wire_fh->object_id) + 1;
		ofs_handle->key_name = gsh_malloc(key_len);
		if (!ofs_handle->key_name) {
			gsh_free(ofs_handle);
			return fsalstat(ERR_FSAL_NOMEM, ENOMEM);
		}
		snprintf(ofs_handle->key_name, key_len, "obj_%lu", wire_fh->object_id);
	}
	
	*obj_hdl = ofs_handle;
	return fsalstat(ERR_FSAL_NO_ERROR, 0);
}

static void test_ofs_fh_checksum(void)
{
	struct ofs_fh fh;
	uint32_t checksum1, checksum2;
	
	printf("Testing OFS file handle checksum computation...\n");
	
	/* Initialize test handle */
	memset(&fh, 0, sizeof(fh));
	fh.version = OFS_FH_VERSION;
	fh.flags = 0;
	fh.export_id = 12345;
	fh.volume_id = 0x12345678;
	fh.bucket_id = 0x87654321;
	fh.object_id = 0x1234567890ABCDEF;
	fh.generation = 42;
	
	/* Compute checksum */
	checksum1 = ofs_compute_fh_checksum(&fh);
	assert(checksum1 != 0);
	
	/* Compute checksum again - should be same */
	checksum2 = ofs_compute_fh_checksum(&fh);
	assert(checksum1 == checksum2);
	
	/* Modify a field and verify checksum changes */
	fh.generation = 43;
	checksum2 = ofs_compute_fh_checksum(&fh);
	assert(checksum1 != checksum2);
	
	printf("File handle checksum tests passed!\n");
}

static void test_ofs_encode_decode_roundtrip(void)
{
	struct ofs_fsal_obj_handle obj_hdl;
	struct ofs_fh wire_fh;
	struct ofs_fsal_obj_handle *decoded_hdl;
	fsal_status_t status;
	
	printf("Testing OFS file handle encode/decode roundtrip...\n");
	
	/* Initialize test object handle */
	memset(&obj_hdl, 0, sizeof(obj_hdl));
	obj_hdl.export = &test_export;
	obj_hdl.volume_id = CityHash32("test-volume", strlen("test-volume"));
	obj_hdl.bucket_id = CityHash32("test-bucket", strlen("test-bucket"));
	obj_hdl.object_id = CityHash64("test-file", strlen("test-file"));
	obj_hdl.generation = 1;
	obj_hdl.key_name = "test-file";
	
	/* Encode to wire format */
	status = ofs_encode_fh(&obj_hdl, &wire_fh);
	assert(!FSAL_IS_ERROR(status));
	assert(wire_fh.version == OFS_FH_VERSION);
	assert(wire_fh.export_id == 12345);
	assert(wire_fh.volume_id == obj_hdl.volume_id);
	assert(wire_fh.bucket_id == obj_hdl.bucket_id);
	assert(wire_fh.object_id == obj_hdl.object_id);
	assert(wire_fh.generation == obj_hdl.generation);
	assert(wire_fh.checksum != 0);
	
	/* Decode back to object handle */
	status = ofs_decode_fh(&test_export.export, &wire_fh, &decoded_hdl);
	assert(!FSAL_IS_ERROR(status));
	assert(decoded_hdl != NULL);
	assert(decoded_hdl->volume_id == obj_hdl.volume_id);
	assert(decoded_hdl->bucket_id == obj_hdl.bucket_id);
	assert(decoded_hdl->object_id == obj_hdl.object_id);
	assert(decoded_hdl->generation == obj_hdl.generation);
	
	/* Clean up */
	gsh_free(decoded_hdl->key_name);
	gsh_free(decoded_hdl);
	
	printf("Encode/decode roundtrip tests passed!\n");
}

static void test_ofs_handle_corruption_detection(void)
{
	struct ofs_fsal_obj_handle obj_hdl;
	struct ofs_fh wire_fh;
	struct ofs_fsal_obj_handle *decoded_hdl;
	fsal_status_t status;
	
	printf("Testing OFS file handle corruption detection...\n");
	
	/* Initialize test object handle */
	memset(&obj_hdl, 0, sizeof(obj_hdl));
	obj_hdl.export = &test_export;
	obj_hdl.volume_id = 0x12345678;
	obj_hdl.bucket_id = 0x87654321;
	obj_hdl.object_id = 0x1234567890ABCDEF;
	obj_hdl.generation = 100;
	obj_hdl.key_name = "test-file";
	
	/* Encode to wire format */
	status = ofs_encode_fh(&obj_hdl, &wire_fh);
	assert(!FSAL_IS_ERROR(status));
	
	/* Test corruption: Invalid version */
	wire_fh.version = 0xFF;
	status = ofs_decode_fh(&test_export.export, &wire_fh, &decoded_hdl);
	assert(FSAL_IS_ERROR(status));
	assert(status.major == ERR_FSAL_BADHANDLE);
	
	/* Restore valid version */
	wire_fh.version = OFS_FH_VERSION;
	
	/* Test corruption: Wrong export ID */
	wire_fh.export_id = 99999;
	status = ofs_decode_fh(&test_export.export, &wire_fh, &decoded_hdl);
	assert(FSAL_IS_ERROR(status));
	assert(status.major == ERR_FSAL_BADHANDLE);
	
	/* Restore valid export ID */
	wire_fh.export_id = 12345;
	
	/* Test corruption: Invalid checksum */
	wire_fh.checksum ^= 0xFF;
	status = ofs_decode_fh(&test_export.export, &wire_fh, &decoded_hdl);
	assert(FSAL_IS_ERROR(status));
	assert(status.major == ERR_FSAL_BADHANDLE);
	
	/* Test corruption: Corrupt data field */
	wire_fh.checksum = ofs_compute_fh_checksum(&wire_fh); /* Valid checksum */
	wire_fh.object_id ^= 0xFF; /* But corrupt data */
	status = ofs_decode_fh(&test_export.export, &wire_fh, &decoded_hdl);
	assert(FSAL_IS_ERROR(status));
	assert(status.major == ERR_FSAL_BADHANDLE);
	
	printf("Corruption detection tests passed!\n");
}

static void test_ofs_cross_export_rejection(void)
{
	struct ofs_fsal_obj_handle obj_hdl;
	struct ofs_fh wire_fh;
	struct ofs_fsal_obj_handle *decoded_hdl;
	struct ofs_fsal_export other_export;
	fsal_status_t status;
	
	printf("Testing OFS cross-export handle rejection...\n");
	
	/* Setup another export with different ID */
	other_export = test_export;
	other_export.export.export_id = 54321;
	
	/* Initialize test object handle */
	memset(&obj_hdl, 0, sizeof(obj_hdl));
	obj_hdl.export = &test_export;
	obj_hdl.volume_id = 0x12345678;
	obj_hdl.bucket_id = 0x87654321;
	obj_hdl.object_id = 0x1234567890ABCDEF;
	obj_hdl.generation = 1;
	obj_hdl.key_name = "test-file";
	
	/* Encode with first export */
	status = ofs_encode_fh(&obj_hdl, &wire_fh);
	assert(!FSAL_IS_ERROR(status));
	
	/* Try to decode with second export - should fail */
	status = ofs_decode_fh(&other_export.export, &wire_fh, &decoded_hdl);
	assert(FSAL_IS_ERROR(status));
	assert(status.major == ERR_FSAL_BADHANDLE);
	
	/* Decode with correct export - should succeed */
	status = ofs_decode_fh(&test_export.export, &wire_fh, &decoded_hdl);
	assert(!FSAL_IS_ERROR(status));
	assert(decoded_hdl != NULL);
	
	/* Clean up */
	gsh_free(decoded_hdl->key_name);
	gsh_free(decoded_hdl);
	
	printf("Cross-export rejection tests passed!\n");
}

static void test_ofs_stable_across_rename(void)
{
	struct ofs_fsal_obj_handle obj_hdl1, obj_hdl2;
	struct ofs_fh wire_fh1, wire_fh2;
	fsal_status_t status;
	
	printf("Testing OFS handle stability across rename...\n");
	
	/* Initialize first object handle with original name */
	memset(&obj_hdl1, 0, sizeof(obj_hdl1));
	obj_hdl1.export = &test_export;
	obj_hdl1.volume_id = CityHash32("test-volume", strlen("test-volume"));
	obj_hdl1.bucket_id = CityHash32("test-bucket", strlen("test-bucket"));
	obj_hdl1.object_id = CityHash64("original-file", strlen("original-file"));
	obj_hdl1.generation = 1;
	obj_hdl1.key_name = "original-file";
	
	/* Initialize second object handle with renamed key name but same object_id */
	obj_hdl2 = obj_hdl1;
	obj_hdl2.key_name = "renamed-file";
	/* object_id remains the same - this is the key requirement for stability */
	
	/* Encode both handles */
	status = ofs_encode_fh(&obj_hdl1, &wire_fh1);
	assert(!FSAL_IS_ERROR(status));
	
	status = ofs_encode_fh(&obj_hdl2, &wire_fh2);
	assert(!FSAL_IS_ERROR(status));
	
	/* The object_id should be the same (stable across rename) */
	assert(wire_fh1.object_id == wire_fh2.object_id);
	
	/* All other fields should also be the same */
	assert(wire_fh1.version == wire_fh2.version);
	assert(wire_fh1.export_id == wire_fh2.export_id);
	assert(wire_fh1.volume_id == wire_fh2.volume_id);
	assert(wire_fh1.bucket_id == wire_fh2.bucket_id);
	assert(wire_fh1.generation == wire_fh2.generation);
	assert(wire_fh1.checksum == wire_fh2.checksum);
	
	/* The entire handle should be identical */
	assert(memcmp(&wire_fh1, &wire_fh2, sizeof(wire_fh1)) == 0);
	
	printf("Handle stability across rename tests passed!\n");
}

static void test_ofs_wire_format_size(void)
{
	printf("Testing OFS wire format size constraints...\n");
	
	/* Verify handle size is reasonable for NFS */
	assert(sizeof(struct ofs_fh) <= 128); /* NFSv4 max handle size */
	assert(sizeof(struct ofs_fh) <= 64);  /* NFSv3 max handle size */
	
	printf("Wire format size: %zu bytes\n", sizeof(struct ofs_fh));
	printf("Wire format size tests passed!\n");
}

int main(void)
{
	printf("Running OFS file handle encoding/decoding tests...\n\n");
	
	test_ofs_fh_checksum();
	test_ofs_encode_decode_roundtrip();
	test_ofs_handle_corruption_detection();
	test_ofs_cross_export_rejection();
	test_ofs_stable_across_rename();
	test_ofs_wire_format_size();
	
	printf("\nAll OFS file handle tests passed successfully!\n");
	return 0;
}
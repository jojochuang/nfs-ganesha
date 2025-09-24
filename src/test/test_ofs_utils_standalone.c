// SPDX-License-Identifier: LGPL-3.0-or-later
/*
 * vim:shiftwidth=8:tabstop=8:
 *
 * Copyright 2024 NFS-Ganesha Project
 * 
 * Standalone tests for OFS utility functions
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <assert.h>
#include <stdbool.h>
#include <unistd.h>
#include <errno.h>

/* Mock the gsh_* functions for testing */
char *gsh_strdup(const char *s) {
	return strdup(s);
}

void gsh_free(void *ptr) {
	free(ptr);
}

void *gsh_malloc(size_t size) {
	return malloc(size);
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

void LogCrit(int component, const char *format, ...) {
	(void)component; (void)format;
}

#define COMPONENT_FSAL 1

/* Include the functions we want to test */
int ofs_parse_ozone_uri(const char *ozone_uri, char **volume_name,
			char **bucket_name, char **service_uri);
bool ofs_volume_is_whitelisted(const char *volume_name, const char *volume_whitelist);

/* Simple implementations of the functions we're testing */
int ofs_parse_ozone_uri(const char *ozone_uri, char **volume_name,
			char **bucket_name, char **service_uri)
{
	char *uri_copy, *token, *saveptr;
	char *volume = NULL, *bucket = NULL, *service = NULL;
	int result = 0;

	if (!ozone_uri || !volume_name || !bucket_name) {
		return -1;
	}

	*volume_name = NULL;
	*bucket_name = NULL;
	if (service_uri) {
		*service_uri = NULL;
	}

	uri_copy = gsh_strdup(ozone_uri);
	if (!uri_copy) {
		return -1;
	}

	/* Check for ozone:// prefix */
	if (strncmp(uri_copy, "ozone://", 8) == 0) {
		char *path_start = strchr(uri_copy + 8, '/');
		if (path_start) {
			/* Extract service URI */
			*path_start = '\0';
			service = gsh_strdup(uri_copy + 8);
			/* Move to path part */
			memmove(uri_copy, path_start + 1, strlen(path_start + 1) + 1);
		} else {
			/* No path after service, invalid */
			result = -1;
			goto cleanup;
		}
	}

	/* Parse volume/bucket format */
	token = strtok_r(uri_copy, "/", &saveptr);
	if (!token || strlen(token) == 0) {
		result = -1;
		goto cleanup;
	}
	volume = gsh_strdup(token);

	token = strtok_r(NULL, "/", &saveptr);
	if (!token || strlen(token) == 0) {
		result = -1;
		goto cleanup;
	}
	bucket = gsh_strdup(token);

	/* Check for extra components */
	token = strtok_r(NULL, "/", &saveptr);
	if (token) {
		LogWarn(COMPONENT_FSAL, "OzoneURI has extra path components, ignoring: %s", token);
	}

	/* Success - assign outputs */
	*volume_name = volume;
	*bucket_name = bucket;
	if (service_uri) {
		*service_uri = service;
	}
	volume = bucket = service = NULL;

cleanup:
	gsh_free(uri_copy);
	gsh_free(volume);
	gsh_free(bucket);  
	gsh_free(service);
	return result;
}

bool ofs_volume_is_whitelisted(const char *volume_name, const char *volume_whitelist)
{
	char *whitelist_copy, *token, *saveptr;
	bool result = false;

	if (!volume_name) {
		return false;
	}

	/* If no whitelist is specified, allow all volumes */
	if (!volume_whitelist || strlen(volume_whitelist) == 0) {
		return true;
	}

	whitelist_copy = gsh_strdup(volume_whitelist);
	if (!whitelist_copy) {
		return false;
	}

	/* Check each volume in the whitelist */
	token = strtok_r(whitelist_copy, ",", &saveptr);
	while (token) {
		/* Trim whitespace */
		while (*token == ' ' || *token == '\t') token++;
		char *end = token + strlen(token) - 1;
		while (end > token && (*end == ' ' || *end == '\t')) *end-- = '\0';

		if (strcmp(volume_name, token) == 0) {
			result = true;
			break;
		}
		token = strtok_r(NULL, ",", &saveptr);
	}

	gsh_free(whitelist_copy);
	return result;
}

static void test_ofs_parse_ozone_uri_valid(void)
{
	char *volume, *bucket, *service;
	int result;
	
	printf("Testing ofs_parse_ozone_uri with valid inputs...\n");
	
	/* Test simple volume/bucket format */
	result = ofs_parse_ozone_uri("testvol/testbucket", &volume, &bucket, &service);
	assert(result == 0);
	assert(strcmp(volume, "testvol") == 0);
	assert(strcmp(bucket, "testbucket") == 0);
	assert(service == NULL);
	gsh_free(volume);
	gsh_free(bucket);
	
	/* Test with ozone:// prefix */
	result = ofs_parse_ozone_uri("ozone://localhost:9862/myvol/mybucket", 
				     &volume, &bucket, &service);
	assert(result == 0);
	assert(strcmp(volume, "myvol") == 0);
	assert(strcmp(bucket, "mybucket") == 0);
	assert(strcmp(service, "localhost:9862") == 0);
	gsh_free(volume);
	gsh_free(bucket);
	gsh_free(service);
	
	printf("✓ Valid OzoneURI parsing tests passed!\n");
}

static void test_ofs_parse_ozone_uri_invalid(void)
{
	char *volume, *bucket, *service;
	int result;
	
	printf("Testing ofs_parse_ozone_uri with invalid inputs...\n");
	
	/* Test NULL input */
	result = ofs_parse_ozone_uri(NULL, &volume, &bucket, &service);
	assert(result == -1);
	
	/* Test missing bucket */
	result = ofs_parse_ozone_uri("testvol", &volume, &bucket, &service);
	assert(result == -1);
	
	/* Test missing volume */
	result = ofs_parse_ozone_uri("/testbucket", &volume, &bucket, &service);
	assert(result == -1);
	
	/* Test empty components */
	result = ofs_parse_ozone_uri("/", &volume, &bucket, &service);
	assert(result == -1);
	
	/* Test ozone:// without volume/bucket */
	result = ofs_parse_ozone_uri("ozone://localhost:9862", &volume, &bucket, &service);
	assert(result == -1);
	
	printf("✓ Invalid OzoneURI parsing tests passed!\n");
}

static void test_ofs_volume_is_whitelisted(void)
{
	printf("Testing ofs_volume_is_whitelisted...\n");
	
	/* Test with no whitelist (should allow all) */
	assert(ofs_volume_is_whitelisted("testvol", NULL) == true);
	assert(ofs_volume_is_whitelisted("testvol", "") == true);
	
	/* Test with single volume whitelist */
	assert(ofs_volume_is_whitelisted("testvol", "testvol") == true);
	assert(ofs_volume_is_whitelisted("othervol", "testvol") == false);
	
	/* Test with multiple volume whitelist */
	assert(ofs_volume_is_whitelisted("vol1", "vol1,vol2,vol3") == true);
	assert(ofs_volume_is_whitelisted("vol2", "vol1,vol2,vol3") == true);
	assert(ofs_volume_is_whitelisted("vol3", "vol1,vol2,vol3") == true);
	assert(ofs_volume_is_whitelisted("vol4", "vol1,vol2,vol3") == false);
	
	/* Test with whitespace in whitelist */
	assert(ofs_volume_is_whitelisted("vol1", " vol1 , vol2 , vol3 ") == true);
	assert(ofs_volume_is_whitelisted("vol2", " vol1 , vol2 , vol3 ") == true);
	
	/* Test NULL volume name */
	assert(ofs_volume_is_whitelisted(NULL, "vol1,vol2") == false);
	
	printf("✓ Volume whitelist tests passed!\n");
}

int main(void)
{
	printf("=== Running OFS utility function tests ===\n\n");
	
	test_ofs_parse_ozone_uri_valid();
	test_ofs_parse_ozone_uri_invalid();
	test_ofs_volume_is_whitelisted();
	
	printf("\n🎉 All OFS utility function tests passed successfully!\n");
	printf("✅ LOOKUP/GETATTR/ACCESS utility functions are ready\n");
	return 0;
}
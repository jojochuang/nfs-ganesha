// SPDX-License-Identifier: LGPL-3.0-or-later
/*
 * vim:shiftwidth=8:tabstop=8:
 *
 * Copyright 2024 NFS-Ganesha Project
 * 
 * Standalone unit tests for OFS configuration parsing
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <assert.h>
#include <stdbool.h>

/* Mock the gsh_strdup and gsh_free functions for testing */
char *gsh_strdup(const char *s) {
	if (!s) return NULL;
	return strdup(s);
}

void gsh_free(void *ptr) {
	free(ptr);
}

/* Inline the configuration functions for testing */

/**
 * @brief Parse OzoneURI into volume and bucket components
 * 
 * Expected format: "volume/bucket" or "ozone://host:port/volume/bucket"
 */
int ofs_parse_ozone_uri(const char *ozone_uri, char **volume_name, 
			char **bucket_name, char **service_uri)
{
	char *uri_copy, *ptr, *slash;
	
	if (!ozone_uri || !volume_name || !bucket_name) {
		return -1;
	}
	
	*volume_name = NULL;
	*bucket_name = NULL;
	if (service_uri)
		*service_uri = NULL;
	
	uri_copy = gsh_strdup(ozone_uri);
	ptr = uri_copy;
	
	/* Check for ozone:// prefix */
	if (strncmp(ptr, "ozone://", 8) == 0) {
		ptr += 8;
		/* Find the first slash after the host:port */
		slash = strchr(ptr, '/');
		if (!slash) {
			gsh_free(uri_copy);
			return -1;
		}
		
		if (service_uri) {
			*slash = '\0';  /* Terminate host:port */
			*service_uri = gsh_strdup(ptr);
			*slash = '/';   /* Restore slash */
		}
		
		ptr = slash + 1;
	}
	
	/* Find volume/bucket separator */
	slash = strchr(ptr, '/');
	if (!slash) {
		gsh_free(uri_copy);
		return -1;
	}
	
	/* Extract volume name */
	*slash = '\0';
	*volume_name = gsh_strdup(ptr);
	
	/* Extract bucket name */
	*bucket_name = gsh_strdup(slash + 1);
	
	gsh_free(uri_copy);
	
	/* Validate that we have both components */
	if (strlen(*volume_name) == 0 || strlen(*bucket_name) == 0) {
		if (*volume_name) {
			gsh_free(*volume_name);
			*volume_name = NULL;
		}
		if (*bucket_name) {
			gsh_free(*bucket_name);
			*bucket_name = NULL;
		}
		return -1;
	}
	
	return 0;
}

/**
 * @brief Check if a volume is in the whitelist
 */
bool ofs_volume_is_whitelisted(const char *volume_name, const char *volume_whitelist)
{
	char *whitelist_copy, *volume, *save_ptr, *end;
	bool found = false;
	
	/* No whitelist means all volumes are allowed */
	if (!volume_whitelist || strlen(volume_whitelist) == 0) {
		return true;
	}
	
	/* No volume name means deny */
	if (!volume_name) {
		return false;
	}
	
	whitelist_copy = gsh_strdup(volume_whitelist);
	
	/* Check each volume in the comma-separated list */
	volume = strtok_r(whitelist_copy, ",", &save_ptr);
	while (volume != NULL) {
		/* Trim leading whitespace */
		while (*volume == ' ' || *volume == '\t') {
			volume++;
		}
		
		/* Trim trailing whitespace */
		end = volume + strlen(volume) - 1;
		while (end > volume && (*end == ' ' || *end == '\t')) {
			*end = '\0';
			end--;
		}
		
		if (strcmp(volume, volume_name) == 0) {
			found = true;
			break;
		}
		
		volume = strtok_r(NULL, ",", &save_ptr);
	}
	
	gsh_free(whitelist_copy);
	return found;
}

/* Test functions */

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
	
	printf("Valid OzoneURI parsing tests passed!\n");
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
	
	printf("Invalid OzoneURI parsing tests passed!\n");
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
	
	printf("Volume whitelist tests passed!\n");
}

int main(void)
{
	printf("Running OFS configuration tests...\n\n");
	
	test_ofs_parse_ozone_uri_valid();
	test_ofs_parse_ozone_uri_invalid();
	test_ofs_volume_is_whitelisted();
	
	printf("\nAll OFS configuration tests passed successfully!\n");
	return 0;
}
// SPDX-License-Identifier: LGPL-3.0-or-later
/*
 * vim:shiftwidth=8:tabstop=8:
 *
 * Copyright 2024 NFS-Ganesha Project
 * 
 * Unit tests for OFS FSAL configuration parsing
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <assert.h>
#include <stdbool.h>

/* Mock the gsh_strdup and gsh_free functions for testing */
char *gsh_strdup(const char *s) {
	return strdup(s);
}

void gsh_free(void *ptr) {
	free(ptr);
}

/* Include the configuration functions */
#include "../FSAL/OFS/fsal_ofs_internal.h"

/* Function prototypes for functions we're testing */
int ofs_parse_ozone_uri(const char *ozone_uri, char **volume_name, 
			char **bucket_name, char **service_uri);
bool ofs_volume_is_whitelisted(const char *volume_name, const char *volume_whitelist);

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
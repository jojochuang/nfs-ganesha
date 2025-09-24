// SPDX-License-Identifier: LGPL-3.0-or-later
/*
 * vim:shiftwidth=8:tabstop=8:
 *
 * Copyright 2024 NFS-Ganesha Project
 * 
 * Unit tests for OFS FSAL handle operations (LOOKUP/GETATTR/ACCESS)
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <assert.h>
#include <stdbool.h>
#include <unistd.h>
#include <sys/stat.h>
#include <time.h>

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

void LogCrit(int component, const char *format, ...) {
	(void)component; (void)format;
}

#define COMPONENT_FSAL 1

/* Test the utility functions that we can test independently */
#include "../FSAL/OFS/fsal_ofs_internal.h"

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

static void test_ofs_ozone_client_api(void)
{
	struct ozone_client *client = NULL;
	struct ozone_volume *volume = NULL;
	struct ozone_bucket *bucket = NULL;
	struct ozone_key *key = NULL;
	int rc;
	
	printf("Testing OFS Ozone client API...\n");
	
	/* Test connect */
	rc = ofs_ozone_connect("ozone://localhost:9862", &client);
	assert(rc == 0);
	assert(client != NULL);
	
	/* Test get volume */
	rc = ofs_ozone_get_volume(client, "test-volume", &volume);
	assert(rc == 0);
	assert(volume != NULL);
	
	/* Test get bucket */
	rc = ofs_ozone_get_bucket(volume, "test-bucket", &bucket);
	assert(rc == 0);
	assert(bucket != NULL);
	
	/* Test head key for root (should exist) */
	rc = ofs_ozone_head_key(bucket, "/", &key);
	assert(rc == 0);
	assert(key != NULL);
	
	/* Test head key for test file (should exist) */
	rc = ofs_ozone_head_key(bucket, "testfile", &key);
	assert(rc == 0);
	assert(key != NULL);
	
	/* Test head key for non-existent file */
	rc = ofs_ozone_head_key(bucket, "nonexistent", &key);
	assert(rc == -ENOENT);
	
	/* Test disconnect */
	ofs_ozone_disconnect(client);
	
	printf("OFS Ozone client API tests passed!\n");
}

int main(void)
{
	printf("Running OFS handle operations tests...\n\n");
	
	test_ofs_parse_ozone_uri_valid();
	test_ofs_parse_ozone_uri_invalid();
	test_ofs_volume_is_whitelisted();
	test_ofs_ozone_client_api();
	
	printf("\nAll OFS handle operations tests passed successfully!\n");
	return 0;
}
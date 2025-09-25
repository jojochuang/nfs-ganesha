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

/* fsal_ofs_util.c
 * OFS FSAL utility functions including Ozone client API wrappers
 */

#include "config.h"
#include "fsal.h"
#include "fsal_ofs_internal.h"
#include "nfs_core.h"
#include <string.h>
#include <stdlib.h>

/* Mock Ozone client structures for initial implementation */
struct ozone_client {
	char *service_uri;
	bool connected;
};

struct ozone_volume {
	struct ozone_client *client;
	char *name;
};

struct ozone_bucket {
	struct ozone_volume *volume;
	char *name;
};

struct ozone_key {
	struct ozone_bucket *bucket;
	char *name;
	size_t size;
	time_t mtime;
	mode_t mode;
	uid_t uid;
	gid_t gid;
};

/**
 * @brief Parse OzoneURI into components
 *
 * @param[in]  ozone_uri     URI to parse (e.g., "volume/bucket" or "ozone://host:port/volume/bucket")
 * @param[out] volume_name   Volume name
 * @param[out] bucket_name   Bucket name  
 * @param[out] service_uri   Service URI (optional, can be NULL)
 *
 * @return 0 on success, -1 on error
 */
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

/**
 * @brief Check if volume is whitelisted
 *
 * @param[in] volume_name      Volume name to check
 * @param[in] volume_whitelist Comma-separated list of allowed volumes
 *
 * @return true if volume is allowed, false otherwise
 */
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

/**
 * @brief Connect to Ozone service
 *
 * @param[in]  service_id  Ozone service identifier/URI
 * @param[out] client      Returned client handle
 *
 * @return 0 on success, error code on failure
 */
int ofs_ozone_connect(const char *service_id, struct ozone_client **client)
{
	struct ozone_client *new_client;

	if (!service_id || !client) {
		return -EINVAL;
	}

	LogInfo(COMPONENT_FSAL, "OFS: Connecting to Ozone service: %s", service_id);

	new_client = gsh_malloc(sizeof(struct ozone_client));
	if (!new_client) {
		return -ENOMEM;
	}

	new_client->service_uri = gsh_strdup(service_id);
	if (!new_client->service_uri) {
		gsh_free(new_client);
		return -ENOMEM;
	}
	new_client->connected = true;

	*client = new_client;

	LogInfo(COMPONENT_FSAL, "OFS: Successfully connected to Ozone service");
	return 0;
}

/**
 * @brief Get Ozone volume handle
 *
 * @param[in]  client       Ozone client
 * @param[in]  volume_name  Volume name
 * @param[out] volume       Returned volume handle
 *
 * @return 0 on success, error code on failure  
 */
int ofs_ozone_get_volume(struct ozone_client *client, const char *volume_name,
			 struct ozone_volume **volume)
{
	struct ozone_volume *new_volume;

	if (!client || !volume_name || !volume) {
		return -EINVAL;
	}

	if (!client->connected) {
		return -ENOTCONN;
	}

	LogDebug(COMPONENT_FSAL, "OFS: Getting volume: %s", volume_name);

	new_volume = gsh_malloc(sizeof(struct ozone_volume));
	if (!new_volume) {
		return -ENOMEM;
	}

	new_volume->client = client;
	new_volume->name = gsh_strdup(volume_name);
	if (!new_volume->name) {
		gsh_free(new_volume);
		return -ENOMEM;
	}

	*volume = new_volume;
	return 0;
}

/**
 * @brief Get Ozone bucket handle
 *
 * @param[in]  volume      Ozone volume
 * @param[in]  bucket_name Bucket name
 * @param[out] bucket      Returned bucket handle
 *
 * @return 0 on success, error code on failure
 */
int ofs_ozone_get_bucket(struct ozone_volume *volume, const char *bucket_name,
			 struct ozone_bucket **bucket)
{
	struct ozone_bucket *new_bucket;

	if (!volume || !bucket_name || !bucket) {
		return -EINVAL;
	}

	LogDebug(COMPONENT_FSAL, "OFS: Getting bucket: %s", bucket_name);

	new_bucket = gsh_malloc(sizeof(struct ozone_bucket));
	if (!new_bucket) {
		return -ENOMEM;
	}

	new_bucket->volume = volume;
	new_bucket->name = gsh_strdup(bucket_name);
	if (!new_bucket->name) {
		gsh_free(new_bucket);
		return -ENOMEM;
	}

	*bucket = new_bucket;
	return 0;
}

/**
 * @brief Head (stat) an Ozone key
 *
 * @param[in]  bucket    Ozone bucket
 * @param[in]  key_name  Key name to head
 * @param[out] key       Returned key handle with metadata
 *
 * @return 0 on success, error code on failure
 */
int ofs_ozone_head_key(struct ozone_bucket *bucket, const char *key_name,
		       struct ozone_key **key)
{
	struct ozone_key *new_key;
	
	if (!bucket || !key_name || !key) {
		return -EINVAL;
	}

	LogDebug(COMPONENT_FSAL, "OFS: Heading key: %s", key_name);

	/* For mock implementation, only allow root directory and some test files */
	if (strcmp(key_name, "/") == 0) {
		/* Root directory always exists */
	} else if (strncmp(key_name, "test", 4) == 0) {
		/* Allow keys starting with "test" for testing */
	} else {
		LogInfo(COMPONENT_FSAL, "OFS: Key not found (mock): %s", key_name);
		return -ENOENT;
	}

	new_key = gsh_malloc(sizeof(struct ozone_key));
	if (!new_key) {
		return -ENOMEM;
	}

	new_key->bucket = bucket;
	new_key->name = gsh_strdup(key_name);
	if (!new_key->name) {
		gsh_free(new_key);
		return -ENOMEM;
	}

	/* Set mock metadata */
	if (strcmp(key_name, "/") == 0) {
		new_key->size = 4096;
		new_key->mode = S_IFDIR | 0755;
	} else {
		new_key->size = 1024;
		new_key->mode = S_IFREG | 0644;
	}
	new_key->mtime = time(NULL);
	new_key->uid = getuid();
	new_key->gid = getgid();

	*key = new_key;
	LogDebug(COMPONENT_FSAL, "OFS: Successfully headed key: %s", key_name);
	return 0;
}

/**
 * @brief Disconnect from Ozone service
 *
 * @param[in] client  Ozone client to disconnect
 */
void ofs_ozone_disconnect(struct ozone_client *client)
{
	if (client) {
		LogInfo(COMPONENT_FSAL, "OFS: Disconnecting from Ozone service: %s",
			client->service_uri ? client->service_uri : "(unknown)");
		
		gsh_free(client->service_uri);
		client->connected = false;
		gsh_free(client);
	}
}
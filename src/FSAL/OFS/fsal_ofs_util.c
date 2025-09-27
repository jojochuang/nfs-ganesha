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
#include <sys/stat.h>
#include <time.h>
#include <unistd.h>
#include <errno.h>

/* libhdfs is required for OFS FSAL */
#include <hdfs.h>

/* Real implementation using libhdfs for Ozone */

/**
 * Ozone client connection using libhdfs
 * In Ozone, we connect via o3fs (Ozone FileSystem) which uses libhdfs
 */
struct ozone_client {
	char *service_uri;		/* Ozone service URI */
	hdfsFS fs;			/* HDFS filesystem handle for o3fs */
	bool connected;			/* Connection status */
};

struct ozone_volume {
	struct ozone_client *client;	/* Back pointer to client */
	char *name;			/* Volume name */
	char *o3fs_path;		/* Full o3fs:// path to volume */
};

struct ozone_bucket {
	struct ozone_volume *volume;	/* Back pointer to volume */
	char *name;			/* Bucket name */
	char *o3fs_path;		/* Full o3fs:// path to bucket */
};

struct ozone_key {
	struct ozone_bucket *bucket;	/* Back pointer to bucket */
	char *name;			/* Key name */
	char *full_path;		/* Full path in o3fs */
	/* Metadata from hdfsFileInfo */
	size_t size;			/* File size */
	time_t mtime;			/* Modification time */
	mode_t mode;			/* File mode */
	uid_t uid;			/* User ID */
	gid_t gid;			/* Group ID */
	short replication;		/* Replication factor */
	tOffset block_size;		/* Block size */
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

	/* 
	 * Connect to Ozone via o3fs (Ozone FileSystem)
	 * o3fs URIs are of the form: o3fs://bucket.volume.service_id/path
	 * For the connection, we create an HDFS connection to the Ozone service
	 */
	
	/* Parse service_id to construct o3fs connection parameters */
	char *host = NULL;
	int port = 9862; /* Default Ozone OM port */
	
	/* Parse service_id like "ozone://hostname:port" or "hostname:port" */
	const char *uri_to_parse = service_id;
	if (strncmp(service_id, "ozone://", 8) == 0) {
		uri_to_parse = service_id + 8;
	}
	
	char *port_sep = strchr(uri_to_parse, ':');
	if (port_sep) {
		size_t host_len = port_sep - uri_to_parse;
		host = gsh_malloc(host_len + 1);
		if (!host) {
			gsh_free(new_client->service_uri);
			gsh_free(new_client);
			return -ENOMEM;
		}
		memcpy(host, uri_to_parse, host_len);
		host[host_len] = '\0';
		port = atoi(port_sep + 1);
	} else {
		host = gsh_strdup(uri_to_parse);
	}

	if (!host) {
		gsh_free(new_client->service_uri);
		gsh_free(new_client);
		return -ENOMEM;
	}

	/* Connect using libhdfs - for Ozone we connect to the O3FS scheme */
	struct hdfsBuilder *builder = hdfsNewBuilder();
	if (!builder) {
		LogCrit(COMPONENT_FSAL, "OFS: Failed to create HDFS builder");
		gsh_free(host);
		gsh_free(new_client->service_uri);
		gsh_free(new_client);
		return -EIO;
	}

	hdfsBuilderSetNameNode(builder, host);
	hdfsBuilderSetNameNodePort(builder, port);
	
	/* Set Ozone-specific configuration */
	hdfsBuilderConfSetStr(builder, "fs.defaultFS", service_id);
	hdfsBuilderConfSetStr(builder, "fs.o3fs.impl", "org.apache.hadoop.fs.ozone.OzoneFileSystem");
	
	new_client->fs = hdfsBuilderConnect(builder);
	if (!new_client->fs) {
		LogCrit(COMPONENT_FSAL, "OFS: Failed to connect to Ozone service %s", service_id);
		hdfsFreeBuilder(builder);  /* Free builder on connection failure */
		gsh_free(host);
		gsh_free(new_client->service_uri);
		gsh_free(new_client);
		return -ECONNREFUSED;
	}
	/* hdfsBuilderConnect takes ownership of builder on success, so don't free it */
	
	gsh_free(host);
	new_client->connected = true;
	
	LogInfo(COMPONENT_FSAL, "OFS: Successfully connected to Ozone service via libhdfs");

	*client = new_client;
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

	/* 
	 * In Ozone with libhdfs, volumes are logical groupings.
	 * The o3fs path for a volume is: o3fs://bucket.volume.service/path
	 * For volume validation, we construct the o3fs path and check if we can access it.
	 */
	
	/* Create o3fs path for the volume - we'll use this for validation */
	int path_len = snprintf(NULL, 0, "o3fs://%s.%s/", "tmpbucket", volume_name) + 1;
	new_volume->o3fs_path = gsh_malloc(path_len);
	if (!new_volume->o3fs_path) {
		gsh_free(new_volume->name);
		gsh_free(new_volume);
		return -ENOMEM;
	}
	snprintf(new_volume->o3fs_path, path_len, "o3fs://%s.%s/", "tmpbucket", volume_name);
	
	/* 
	 * In a real implementation, we might validate the volume exists by:
	 * 1. Using Ozone REST API calls to check volume existence
	 * 2. Or trying to list the volume contents
	 * For now, we assume the volume exists if we got this far.
	 */
	
	LogDebug(COMPONENT_FSAL, "OFS: Volume %s mapped to o3fs path: %s", 
	         volume_name, new_volume->o3fs_path);

	*volume = new_volume;
	LogDebug(COMPONENT_FSAL, "OFS: Successfully retrieved volume: %s", volume_name);
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

	/* 
	 * Construct the o3fs path for this bucket.
	 * O3FS paths are: o3fs://bucket.volume.service/path
	 * The bucket becomes the filesystem root for operations.
	 */
	
	char *service_host = volume->client->service_uri;
	/* Skip ozone:// prefix if present */
	if (strncmp(service_host, "ozone://", 8) == 0) {
		service_host += 8;
	}
	
	int path_len = snprintf(NULL, 0, "o3fs://%s.%s.%s/", 
	                       bucket_name, volume->name, service_host) + 1;
	new_bucket->o3fs_path = gsh_malloc(path_len);
	if (!new_bucket->o3fs_path) {
		gsh_free(new_bucket->name);
		gsh_free(new_bucket);
		return -ENOMEM;
	}
	snprintf(new_bucket->o3fs_path, path_len, "o3fs://%s.%s.%s/", 
	        bucket_name, volume->name, service_host);
	
	/*
	 * In a real implementation, we might validate the bucket exists:
	 * 1. Try to stat the bucket root path
	 * 2. Use Ozone REST API to verify bucket existence
	 * For now, we assume success if we got this far.
	 */
	
	LogDebug(COMPONENT_FSAL, "OFS: Bucket %s mapped to o3fs path: %s", 
	         bucket_name, new_bucket->o3fs_path);

	*bucket = new_bucket;
	LogDebug(COMPONENT_FSAL, "OFS: Successfully retrieved bucket: %s", bucket_name);
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

	/* 
	 * Real implementation using libhdfs to stat the key/object in Ozone
	 * We construct the full o3fs path and use hdfsGetPathInfo to get metadata
	 */
	
	/* Construct full path: bucket_path + key_name */
	size_t full_path_len = strlen(bucket->o3fs_path) + strlen(key_name) + 1;
	new_key->full_path = gsh_malloc(full_path_len);
	if (!new_key->full_path) {
		gsh_free(new_key->name);
		gsh_free(new_key);
		return -ENOMEM;
	}
	
	/* Remove trailing slash from bucket path if key doesn't start with slash */
	if (key_name[0] != '/' && bucket->o3fs_path[strlen(bucket->o3fs_path)-1] == '/') {
		snprintf(new_key->full_path, full_path_len, "%s%s", bucket->o3fs_path, key_name);
	} else if (key_name[0] == '/' && bucket->o3fs_path[strlen(bucket->o3fs_path)-1] != '/') {
		snprintf(new_key->full_path, full_path_len, "%s%s", bucket->o3fs_path, key_name);
	} else if (key_name[0] == '/' && bucket->o3fs_path[strlen(bucket->o3fs_path)-1] == '/') {
		/* Remove one of the slashes */
		snprintf(new_key->full_path, full_path_len, "%s%s", bucket->o3fs_path, key_name + 1);
	} else {
		snprintf(new_key->full_path, full_path_len, "%s/%s", bucket->o3fs_path, key_name);
	}
	
	/* Use libhdfs to get file information */
	hdfsFileInfo *file_info = hdfsGetPathInfo(bucket->volume->client->fs, new_key->full_path);
	if (!file_info) {
		/* File/key doesn't exist */
		LogInfo(COMPONENT_FSAL, "OFS: Key not found: %s (path: %s)", 
		        key_name, new_key->full_path);
		gsh_free(new_key->full_path);
		gsh_free(new_key->name);
		gsh_free(new_key);
		return -ENOENT;
	}
	
	/* Extract metadata from hdfsFileInfo */
	new_key->size = file_info->mSize;
	new_key->mtime = file_info->mLastMod / 1000; /* Convert from milliseconds */
	new_key->uid = 0; /* libhdfs may not provide meaningful UIDs for Ozone */
	new_key->gid = 0;
	new_key->replication = file_info->mReplication;
	new_key->block_size = file_info->mBlockSize;
	
	/* Map file kind to mode */
	if (file_info->mKind == kObjectKindFile) {
		new_key->mode = S_IFREG | 0644;
	} else if (file_info->mKind == kObjectKindDirectory) {
		new_key->mode = S_IFDIR | 0755;
	} else {
		new_key->mode = S_IFREG | 0644; /* Default to regular file */
	}
	
	/* Free the hdfsFileInfo */
	hdfsFreeFileInfo(file_info, 1);
	
	LogDebug(COMPONENT_FSAL, "OFS: Successfully headed key %s: size=%zu, mtime=%ld, mode=0%o", 
	         key_name, new_key->size, new_key->mtime, new_key->mode);

	*key = new_key;
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
		
		/* Close the HDFS filesystem connection */
		if (client->fs) {
			int disconnect_result = hdfsDisconnect(client->fs);
			if (disconnect_result != 0) {
				LogWarn(COMPONENT_FSAL, "OFS: Warning - HDFS disconnect returned error: %d", 
				        disconnect_result);
			}
			client->fs = NULL;
		}
		LogDebug(COMPONENT_FSAL, "OFS: HDFS connection closed");
		
		gsh_free(client->service_uri);
		client->connected = false;
		gsh_free(client);
		
		LogInfo(COMPONENT_FSAL, "OFS: Successfully disconnected from Ozone service");
	}
}
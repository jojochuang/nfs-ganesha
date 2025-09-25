/* SPDX-License-Identifier: LGPL-3.0-or-later */
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

/* OFS internal definitions */

#include "fsal.h"

/* Forward declarations for Ozone client types */
struct ozone_client;
struct ozone_volume;
struct ozone_bucket;
struct ozone_key;

/**
 * @brief OFS FSAL configuration parameters
 */
struct ofs_conf {
	char *ozone_uri;		/* OzoneURI (default-volume, default-bucket) */
	char *volume_whitelist;		/* VolumeWhitelist */
	char *staging_dir;		/* StagingDir */
	uint64_t max_staging_bytes;	/* MaxStagingBytes */
	uint32_t read_ahead_kb;		/* ReadAheadKB */
};

/**
 * OFS internal export
 */
struct ofs_fsal_export {
	struct fsal_export export;	/* Export this wraps */
	char *export_path;		/* The path for this export */
	struct ozone_client *client;	/* Ozone client connection */
	char *volume_name;		/* Ozone volume name */
	char *bucket_name;		/* Ozone bucket name */
};

/**
 * OFS internal object handle
 */
struct ofs_fsal_obj_handle {
	struct fsal_obj_handle obj_handle; /* Base handle */
	char *key_name;			   /* Ozone key name */
	struct ofs_fsal_export *export;   /* Back pointer to export */
};

/* Function prototypes */
void ofs_export_ops_init(struct export_ops *ops);
fsal_status_t ofs_create_export(struct fsal_module *fsal_hdl, void *parse_node,
				struct config_error_type *err_type,
				const struct fsal_up_vector *up_ops);

/* Configuration function prototypes */
struct config_error_type;
int ofs_parse_ozone_uri(const char *ozone_uri, char **volume_name, 
			char **bucket_name, char **service_uri);
bool ofs_volume_is_whitelisted(const char *volume_name, const char *volume_whitelist);

/* Ozone client API wrapper functions (to be implemented) */
int ofs_ozone_connect(const char *service_id, struct ozone_client **client);
int ofs_ozone_get_volume(struct ozone_client *client, const char *volume_name, 
			 struct ozone_volume **volume);
int ofs_ozone_get_bucket(struct ozone_volume *volume, const char *bucket_name,
			 struct ozone_bucket **bucket);
int ofs_ozone_head_key(struct ozone_bucket *bucket, const char *key_name,
		       struct ozone_key **key);
void ofs_ozone_disconnect(struct ozone_client *client);

/* Handle operations function prototypes */
void ofs_handle_ops_init(struct fsal_obj_ops *ops);
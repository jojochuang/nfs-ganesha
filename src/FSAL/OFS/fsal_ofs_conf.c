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

/* fsal_ofs_conf.c
 * OFS FSAL configuration utilities
 */

#include "config.h"
#include "fsal.h"
#include "fsal_ofs_internal.h"
#include <string.h>
#include <stdlib.h>

/**
 * @brief Parse OzoneURI into volume and bucket components
 * 
 * Expected format: "volume/bucket" or "ozone://host:port/volume/bucket"
 *
 * @param[in]  ozone_uri      The OzoneURI to parse
 * @param[out] volume_name    Extracted volume name (caller must free)
 * @param[out] bucket_name    Extracted bucket name (caller must free)
 * @param[out] service_uri    Extracted service URI (caller must free, may be NULL)
 * 
 * @return 0 on success, -1 on error
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
 *
 * @param[in] volume_name      Volume to check
 * @param[in] volume_whitelist Comma-separated list of allowed volumes (may be NULL)
 *
 * @return true if allowed (or no whitelist), false if denied
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
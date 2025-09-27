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

/* fsal_ofs_init.c
 * Module core functions
 */

#include "config.h"

#include "fsal.h"
#include <libgen.h>
#include <pthread.h>
#include <string.h>
#include <limits.h>
#include <sys/types.h>
#include <inttypes.h>
#include "FSAL/fsal_init.h"
#include "FSAL/fsal_commonlib.h"
#include "FSAL/fsal_config.h"
#include "fsal_convert.h"
#include "config_parsing.h"
#include "../fsal_private.h"
#include "fsal_ofs_internal.h"
#include "nfs_exports.h"
#include "export_mgr.h"

/* OFS FSAL module private storage */

/* defined the set of attributes supported with POSIX */
#define OFS_SUPPORTED_ATTRIBUTES (ATTRS_POSIX)

static const char ofsname[] = "OFS";

/* my module private storage */
struct ofs_fsal_module {
	struct fsal_module fsal;
	struct ofs_conf conf;		/* OFS configuration */
};

static struct ofs_fsal_module OFS = {
	.fsal = {
		.fs_info = {
			.maxfilesize = INT64_MAX,
			.maxlink = 0,
			.maxnamelen = MAXNAMLEN,
			.maxpathlen = MAXPATHLEN,
			.no_trunc = true,
			.chown_restricted = true,
			.case_insensitive = false,
			.case_preserving = true,
			.link_support = false,
			.symlink_support = false,
			.lock_support = false,
			.lock_support_async_block = false,
			.named_attr = false,
			.unique_handles = true,
			.acl_support = 0,
			.cansettime = true,
			.homogenous = true,
			.supported_attrs = OFS_SUPPORTED_ATTRIBUTES,
			.maxread = FSAL_MAXIOSIZE,
			.maxwrite = FSAL_MAXIOSIZE,
			.umask = 0,
			.auth_exportpath_xdev = false,
			.link_supports_permission_checks = false,
			.readdir_plus = true,
			.expire_time_parent = -1,
		}
	}
};

/**
 * @brief Validate OFS FSAL configuration
 *
 * @param[in] conf      Configuration to validate
 * @param[out] err_type Error type
 *
 * @return 0 on success, error count on failure
 */
static int ofs_conf_validate(struct ofs_conf *conf,
			     struct config_error_type *err_type)
{
	int errors = 0;

	/* Validate OzoneURI format if provided */
	if (conf->ozone_uri != NULL) {
		/* Basic validation - should contain at least volume/bucket format */
		if (strchr(conf->ozone_uri, '/') == NULL) {
			LogCrit(COMPONENT_FSAL, 
				"OzoneURI must contain volume/bucket format");
			err_type->invalid = true;
			errors++;
		}
	}

	/* Validate staging directory if provided */
	if (conf->staging_dir != NULL) {
		/* Check if directory path is absolute */
		if (conf->staging_dir[0] != '/') {
			LogCrit(COMPONENT_FSAL, 
				"StagingDir must be an absolute path");
			err_type->invalid = true;
			errors++;
		}
	}

	/* Validate MaxStagingBytes (reasonable limits) */
	if (conf->max_staging_bytes > 0) {
		const uint64_t max_reasonable = (uint64_t)1024 * 1024 * 1024 * 1024; /* 1TB */
		if (conf->max_staging_bytes > max_reasonable) {
			LogCrit(COMPONENT_FSAL, 
				"MaxStagingBytes %"PRIu64" exceeds reasonable limit",
				conf->max_staging_bytes);
			err_type->invalid = true;
			errors++;
		}
	}

	/* Validate ReadAheadKB (reasonable limits) */
	if (conf->read_ahead_kb > 0) {
		const uint32_t max_reasonable = 1024 * 1024; /* 1GB */
		if (conf->read_ahead_kb > max_reasonable) {
			LogCrit(COMPONENT_FSAL, 
				"ReadAheadKB %"PRIu32" exceeds reasonable limit",
				conf->read_ahead_kb);
			err_type->invalid = true;
			errors++;
		}
	}

	return errors;
}

/**
 * @brief Initialize OFS configuration with defaults
 *
 * @param[out] conf Configuration structure to initialize
 */
static void ofs_conf_init_defaults(struct ofs_conf *conf)
{
	memset(conf, 0, sizeof(struct ofs_conf));
	
	/* Set default values */
	conf->ozone_uri = NULL;		/* No default URI */
	conf->volume_whitelist = NULL;	/* No default whitelist */
	conf->staging_dir = gsh_strdup("/tmp/nfs-ganesha-ofs"); /* Default staging */
	conf->max_staging_bytes = (uint64_t)1024 * 1024 * 1024; /* 1GB default */
	conf->read_ahead_kb = 1024;	/* 1MB default read-ahead */
}

/**
 * @brief Free OFS configuration strings
 *
 * @param[in] conf Configuration to free
 */
static void ofs_conf_free(struct ofs_conf *conf)
{
	if (conf->ozone_uri) {
		gsh_free(conf->ozone_uri);
		conf->ozone_uri = NULL;
	}
	if (conf->volume_whitelist) {
		gsh_free(conf->volume_whitelist);
		conf->volume_whitelist = NULL;
	}
	if (conf->staging_dir) {
		gsh_free(conf->staging_dir);
		conf->staging_dir = NULL;
	}
}

/**
 * @brief Log effective OFS configuration at INFO level
 *
 * @param[in] conf Configuration to log
 */
static void ofs_conf_log_effective(const struct ofs_conf *conf)
{
	LogInfo(COMPONENT_FSAL, "OFS effective configuration:");
	LogInfo(COMPONENT_FSAL, "  OzoneURI: %s", 
		conf->ozone_uri ? conf->ozone_uri : "(not set)");
	LogInfo(COMPONENT_FSAL, "  VolumeWhitelist: %s",
		conf->volume_whitelist ? conf->volume_whitelist : "(not set)");
	LogInfo(COMPONENT_FSAL, "  StagingDir: %s",
		conf->staging_dir ? conf->staging_dir : "(not set)");
	LogInfo(COMPONENT_FSAL, "  MaxStagingBytes: %"PRIu64" bytes",
		conf->max_staging_bytes);
	LogInfo(COMPONENT_FSAL, "  ReadAheadKB: %"PRIu32" KB",
		conf->read_ahead_kb);
}

/**
 * @brief Commit OFS configuration
 *
 * @param[in] node        Configuration parse node  
 * @param[in] link_mem    Link memory (unused)
 * @param[in] self_struct Configuration structure
 * @param[out] err_type   Error type
 *
 * @return 0 on success, error count on failure
 */
static int ofs_conf_commit(void *node, void *link_mem, void *self_struct,
			   struct config_error_type *err_type)
{
	struct ofs_fsal_module *ofs_module = (struct ofs_fsal_module *)self_struct;
	int errors;
	
	errors = ofs_conf_validate(&ofs_module->conf, err_type);
	if (errors == 0) {
		ofs_conf_log_effective(&ofs_module->conf);
	}
	
	return errors;
}

/* Configuration parameters for OFS FSAL module */
static struct config_item ofs_config_params[] = {
	CONF_ITEM_STR("OzoneURI", 1, 2048, NULL, ofs_fsal_module, 
		      conf.ozone_uri),
	CONF_ITEM_STR("VolumeWhitelist", 1, 4096, NULL, ofs_fsal_module,
		      conf.volume_whitelist),
	CONF_ITEM_PATH("StagingDir", 1, MAXPATHLEN, "/tmp/nfs-ganesha-ofs", ofs_fsal_module,
		       conf.staging_dir),
	CONF_ITEM_UI64("MaxStagingBytes", 0, UINT64_MAX, 1073741824, ofs_fsal_module,
		       conf.max_staging_bytes),
	CONF_ITEM_UI32("ReadAheadKB", 0, UINT32_MAX, 1024, ofs_fsal_module,
		       conf.read_ahead_kb),
	CONFIG_EOL
};

static struct config_block ofs_config_block = {
	.dbus_interface_name = "org.ganesha.nfsd.config.fsal.ofs",
	.blk_desc.name = "OFS",
	.blk_desc.type = CONFIG_BLOCK,
	.blk_desc.flags = CONFIG_UNIQUE,
	.blk_desc.u.blk.init = noop_conf_init,
	.blk_desc.u.blk.params = ofs_config_params,
	.blk_desc.u.blk.commit = ofs_conf_commit
};

static fsal_status_t ofs_init_config(struct fsal_module *fsal_hdl,
				     config_file_t config_struct,
				     struct config_error_type *err_type)
{
	struct ofs_fsal_module *myself = 
		container_of(fsal_hdl, struct ofs_fsal_module, fsal);
	
	LogDebug(COMPONENT_FSAL, "OFS module setup.");

	/* Initialize configuration defaults */
	ofs_conf_init_defaults(&myself->conf);
	
	/* Load configuration from parse tree */
	(void)load_config_from_parse(config_struct, &ofs_config_block, 
				     myself, true, err_type);
	
	if (!config_error_is_harmless(err_type)) {
		ofs_conf_free(&myself->conf);
		return fsalstat(ERR_FSAL_INVAL, 0);
	}

	display_fsinfo(&myself->fsal);

	return fsalstat(ERR_FSAL_NO_ERROR, 0);
}

/* Configuration parameters for OFS export */
static struct config_item ofs_export_params[] = {
	CONF_ITEM_NOOP("name"),
	CONFIG_EOL
};

static struct config_block ofs_export_param_block = {
	.dbus_interface_name = "org.ganesha.nfsd.config.fsal.ofs-export%d",
	.blk_desc.name = "FSAL",
	.blk_desc.type = CONFIG_BLOCK,
	.blk_desc.u.blk.init = noop_conf_init,
	.blk_desc.u.blk.params = ofs_export_params,
	.blk_desc.u.blk.commit = noop_conf_commit
};

/**
 * @brief OFS FSAL export creation
 *
 * @param[in] fsal_hdl     FSAL module handle  
 * @param[in] parse_node   Configuration parse node
 * @param[out] err_type    Error information
 * @param[in] up_ops       Upcall operations
 *
 * @return FSAL status
 */
fsal_status_t ofs_create_export(struct fsal_module *fsal_hdl,
				void *parse_node,
				struct config_error_type *err_type,
				const struct fsal_up_vector *up_ops)
{
	struct ofs_fsal_export *myself;
	int retval = 0;
	fsal_status_t fsal_status = { 0, 0 };

	LogDebug(COMPONENT_FSAL, "OFS create_export called");

	myself = gsh_calloc(1, sizeof(struct ofs_fsal_export));

	fsal_export_init(&myself->export);
	ofs_export_ops_init(&myself->export.exp_ops);

	retval = load_config_from_node(parse_node, &ofs_export_param_block,
				       myself, true, err_type);
	if (retval != 0) {
		LogMajor(COMPONENT_FSAL, 
			 "OFS export config error, code %d", retval);
		fsal_status = posix2fsal_status(EINVAL);
		goto err_free;
	}

	myself->export.fsal = fsal_hdl;

	retval = fsal_attach_export(fsal_hdl, &myself->export.exports);
	if (retval != 0) {
		LogMajor(COMPONENT_FSAL, "Could not attach OFS export");
		fsal_status = posix2fsal_status(retval);
		goto err_free;
	}

	myself->export.up_ops = up_ops;

	/* Save the export path */
	myself->export_path = gsh_strdup(CTX_FULLPATH(op_ctx));
	op_ctx->fsal_export = &myself->export;

	/* Initialize Ozone client connection using module configuration */
	struct ofs_fsal_module *ofs_module = 
		container_of(fsal_hdl, struct ofs_fsal_module, fsal);
	
	char *volume_name = NULL, *bucket_name = NULL, *service_uri = NULL;
	
	/* Parse OzoneURI configuration to extract components */
	if (ofs_module->conf.ozone_uri != NULL) {
		retval = ofs_parse_ozone_uri(ofs_module->conf.ozone_uri, 
					     &volume_name, &bucket_name, &service_uri);
		if (retval != 0) {
			LogCrit(COMPONENT_FSAL, 
				"OFS: Failed to parse OzoneURI: %s", 
				ofs_module->conf.ozone_uri);
			fsal_status = posix2fsal_status(EINVAL);
			goto err_cleanup;
		}
	} else {
		/* Use defaults if no configuration provided */
		volume_name = gsh_strdup("default-volume");
		bucket_name = gsh_strdup("default-bucket");
		service_uri = gsh_strdup("ozone://localhost:9862");
	}
	
	myself->volume_name = volume_name;
	myself->bucket_name = bucket_name;
	
	/* Connect to Ozone service using configured service URI */
	retval = ofs_ozone_connect(service_uri, &myself->client);
	gsh_free(service_uri); /* No longer needed after connection */
	if (retval != 0) {
		LogCrit(COMPONENT_FSAL, 
			"OFS: Failed to connect to Ozone service: %d", retval);
		fsal_status = posix2fsal_status(EIO);
		goto err_cleanup;
	}

	LogInfo(COMPONENT_FSAL, "Created OFS export for %s (volume: %s, bucket: %s)",
		myself->export_path, myself->volume_name, myself->bucket_name);

	return fsalstat(ERR_FSAL_NO_ERROR, 0);

err_cleanup:
	if (myself->export_path)
		gsh_free(myself->export_path);
	if (myself->volume_name)
		gsh_free(myself->volume_name);
	if (myself->bucket_name)
		gsh_free(myself->bucket_name);
	fsal_detach_export(fsal_hdl, &myself->export.exports);

err_free:
	free_export_ops(&myself->export);
	gsh_free(myself);
	return fsal_status;
}

/**
 * @brief Initialize and register the FSAL
 *
 * This function initializes the FSAL module handle.  It exists solely to
 * produce a properly constructed FSAL module handle.
 */
MODULE_INIT void init(void)
{
	int retval;
	struct fsal_module *myself = &OFS.fsal;

	LogDebug(COMPONENT_FSAL, "OFS module registering.");

	retval = register_fsal(myself, ofsname, FSAL_MAJOR_VERSION,
			       FSAL_MINOR_VERSION, FSAL_ID_EXPERIMENTAL);
	if (retval != 0) {
		LogCrit(COMPONENT_FSAL, "OFS module failed to register.");
		return;
	}

	myself->m_ops.create_export = ofs_create_export;
	myself->m_ops.init_config = ofs_init_config;

	LogDebug(COMPONENT_FSAL, "OFS module registered successfully.");
}

MODULE_FINI void finish(void)
{
	int retval;

	LogDebug(COMPONENT_FSAL, "OFS module finishing.");

	retval = unregister_fsal(&OFS.fsal);
	if (retval != 0) {
		LogCrit(COMPONENT_FSAL,
			"Unable to unload OFS FSAL. Dying with extreme prejudice.");
		abort();
	}
}
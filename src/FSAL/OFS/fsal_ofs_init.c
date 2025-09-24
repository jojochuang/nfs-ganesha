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
#include "FSAL/fsal_init.h"
#include "fsal_convert.h"
#include "../fsal_private.h"

/* OFS FSAL module private storage */

/* defined the set of attributes supported with POSIX */
#define OFS_SUPPORTED_ATTRIBUTES (ATTRS_POSIX)

static const char ofsname[] = "OFS";

/* my module private storage */
struct ofs_fsal_module {
	struct fsal_module fsal;
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
 * @brief OFS FSAL configuration initialization
 *
 * @param[in] fsal_hdl      FSAL module handle
 * @param[in] config_struct Configuration structure
 * @param[out] err_type     Error type
 *
 * @return FSAL status
 */
static fsal_status_t ofs_init_config(struct fsal_module *fsal_hdl,
				     config_file_t config_struct,
				     struct config_error_type *err_type)
{
	LogDebug(COMPONENT_FSAL, "OFS module setup.");

	display_fsinfo(&OFS.fsal);

	return fsalstat(ERR_FSAL_NO_ERROR, 0);
}

/**
 * @brief OFS FSAL export creation
 *
 * @param[in] fsal_hdl     FSAL module handle  
 * @param[in] path         Export path
 * @param[in] fs_options   Filesystem options
 * @param[out] export_hdl  Export handle
 * @param[in] attrs_out    Export attributes
 *
 * @return FSAL status
 */
static fsal_status_t ofs_create_export(struct fsal_module *fsal_hdl,
					void *parse_node,
					struct config_error_type *err_type,
					const struct fsal_up_vector *up_ops)
{
	LogDebug(COMPONENT_FSAL, "OFS create_export called - not implemented.");
	return fsalstat(ERR_FSAL_NOTSUPP, 0);
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
// SPDX-License-Identifier: LGPL-3.0-or-later
/*
 * vim:shiftwidth=8:tabstop=8:
 *
 * Copyright 2024 Test Implementation for CI
 * Author: CI Implementation
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
 */

/* main.c
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
#include "ofs_int.h"
#include "fsal_convert.h"
#include "../fsal_private.h"

static const char ofsname[] = "OFS";

/* OFS FSAL module private storage */
struct ofs_fsal_module
	OFS = { .fsal = { .fs_info = {
				  .maxfilesize = INT64_MAX,
				  .maxlink = 0,
				  .maxnamelen = 1024,
				  .maxpathlen = 1024,
				  .no_trunc = true,
				  .chown_restricted = false,
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
			  } } };

/* Module initialization */
MODULE_INIT void ofs_init(void)
{
	int retval;
	struct fsal_module *myself = &OFS.fsal;

	retval = register_fsal(myself, ofsname, FSAL_MAJOR_VERSION,
			       FSAL_MINOR_VERSION, FSAL_ID_NO_PNFS);
	if (retval != 0) {
		fprintf(stderr, "OFS module failed to register");
		return;
	}
	myself->m_ops.create_export = NULL;
	myself->m_ops.update_export = NULL;
}

MODULE_FINI void ofs_unload(void)
{
	int retval;

	retval = unregister_fsal(&OFS.fsal);
	if (retval != 0) {
		fprintf(stderr, "OFS module failed to unregister");
		return;
	}
}
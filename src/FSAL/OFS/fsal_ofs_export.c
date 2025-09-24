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

/* fsal_ofs_export.c
 * OFS FSAL export operations
 */

#include "config.h"
#include "fsal.h"
#include "FSAL/fsal_commonlib.h"
#include "FSAL/fsal_config.h"
#include "fsal_ofs_internal.h"
#include "nfs_exports.h"
#include "export_mgr.h"

/**
 * @brief Release an export
 *
 * @param[in] exp_hdl	Export handle to release
 */
static void ofs_release(struct fsal_export *exp_hdl)
{
	struct ofs_fsal_export *myself;

	myself = container_of(exp_hdl, struct ofs_fsal_export, export);

	LogDebug(COMPONENT_FSAL,
		 "Releasing OFS export %" PRIu16 " for %s",
		 exp_hdl->export_id,
		 myself->export_path ? myself->export_path : "NULL");

	fsal_detach_export(exp_hdl->fsal, &exp_hdl->exports);
	free_export_ops(exp_hdl);

	if (myself->export_path)
		gsh_free(myself->export_path);
	gsh_free(myself);
}

/**
 * @brief Lookup a path
 *
 * @param[in]  exp_hdl   Export handle
 * @param[in]  path      Path to look up
 * @param[out] handle    Handle for the object
 * @param[out] attrs_out Attributes of the object
 *
 * @return FSAL status
 */
static fsal_status_t ofs_lookup_path(struct fsal_export *exp_hdl,
				     const char *path,
				     struct fsal_obj_handle **handle,
				     struct fsal_attrlist *attrs_out)
{
	LogDebug(COMPONENT_FSAL, "OFS lookup_path called for %s - not implemented", path);
	return fsalstat(ERR_FSAL_NOTSUPP, 0);
}

/**
 * @brief Create a handle from wire data
 *
 * @param[in]  exp_hdl    Export handle
 * @param[in]  hdl_desc   Wire handle descriptor
 * @param[out] handle     Handle for the object
 * @param[out] attrs_out  Attributes of the object
 *
 * @return FSAL status
 */
static fsal_status_t ofs_create_handle(struct fsal_export *exp_hdl,
				       struct gsh_buffdesc *hdl_desc,
				       struct fsal_obj_handle **handle,
				       struct fsal_attrlist *attrs_out)
{
	LogDebug(COMPONENT_FSAL, "OFS create_handle called - not implemented");
	return fsalstat(ERR_FSAL_NOTSUPP, 0);
}

/**
 * @brief Initialize export ops vector
 *
 * @param[in,out] ops   Vector of export operations
 */
void ofs_export_ops_init(struct export_ops *ops)
{
	ops->release = ofs_release;
	ops->lookup_path = ofs_lookup_path;
	ops->create_handle = ofs_create_handle;
}
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

/**
 * OFS internal export
 */
struct ofs_fsal_export {
	struct fsal_export export;	/* Export this wraps */
	char *export_path;		/* The path for this export */
};

/* Function prototypes */
void ofs_export_ops_init(struct export_ops *ops);
fsal_status_t ofs_create_export(struct fsal_module *fsal_hdl, void *parse_node,
				struct config_error_type *err_type,
				const struct fsal_up_vector *up_ops);
// SPDX-License-Identifier: LGPL-3.0-or-later
/*
 * vim:shiftwidth=8:tabstop=8:
 *
 * Copyright 2024 Test Implementation for CI
 * Author: CI Implementation
 *
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

#ifndef OFS_INT_H
#define OFS_INT_H

#include "fsal.h"
#include "fsal_types.h"
#include "FSAL/fsal_commonlib.h"

#define OFS_SUPPORTED_ATTRIBUTES (ATTRS_POSIX)

struct ofs_fsal_module {
	struct fsal_module fsal;
};

struct ofs_export {
	struct fsal_export export;
};

struct ofs_handle {
	struct fsal_obj_handle handle;
};

extern struct ofs_fsal_module OFS;

#endif /* OFS_INT_H */
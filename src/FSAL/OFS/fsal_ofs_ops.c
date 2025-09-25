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

/* fsal_ofs_ops.c
 * OFS FSAL additional operations (future implementations)
 */

#include "config.h"
#include "fsal.h"
#include "fsal_ofs_internal.h"

/**
 * @brief Placeholder for additional OFS FSAL operations
 *
 * This file can be used for implementing additional FSAL operations
 * beyond the core LOOKUP/GETATTR/ACCESS that are implemented in
 * fsal_ofs_handle.c
 *
 * Future operations to implement:
 * - readdir
 * - open/close
 * - read/write
 * - create/mkdir
 * - unlink/rmdir
 * - rename
 * - symlink
 * - setattr
 * - etc.
 */

/* For now, this file just needs to exist to satisfy the CMakeLists.txt build */
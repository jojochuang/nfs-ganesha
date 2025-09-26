# SPDX-License-Identifier: LGPL-3.0-or-later
#-------------------------------------------------------------------------------
#
# Copyright 2024, NFS-Ganesha Project
# Contributor: GitHub Copilot
#
# This program is free software; you can redistribute it and/or
# modify it under the terms of the GNU Lesser General Public
# License as published by the Free Software Foundation; either
# version 3 of the License, or (at your option) any later version.
#
# This program is distributed in the hope that it will be useful,
# but WITHOUT ANY WARRANTY; without even the implied warranty of
# MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
# Lesser General Public License for more details.
#
# You should have received a copy of the GNU Lesser General Public
# License along with this library; if not, write to the Free Software
# Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA  02110-1301  USA
#
#-------------------------------------------------------------------------------

add_definitions(
  -D__USE_GNU
)

set( LIB_PREFIX 64)

# libhdfs dependency is checked in main CMakeLists.txt
# At this point, we know HDFS_INCLUDE_DIR and HDFS_LIBRARY are available
add_definitions(-DUSE_LIBHDFS)

########### next target ###############

SET(fsalofs_LIB_SRCS
   fsal_ofs_init.c
   fsal_ofs_export.c
   fsal_ofs_ops.c
   fsal_ofs_handle.c
   fsal_ofs_conf.c
   fsal_ofs_util.c
)

add_library(fsalofs MODULE ${fsalofs_LIB_SRCS})
add_sanitizers(fsalofs)

# Include hdfs directories and link libraries (required)
target_include_directories(fsalofs PRIVATE ${HDFS_INCLUDE_DIR})

target_link_libraries(fsalofs
  ganesha_nfsd
  ${HDFS_LIBRARY}
  ${SYSTEM_LIBRARIES}
  ${LTTNG_LIBRARIES}
  ${LDFLAG_DISALLOW_UNDEF}
)

set_target_properties(fsalofs PROPERTIES VERSION 4.2.0 SOVERSION 4)
install(TARGETS fsalofs COMPONENT fsal DESTINATION ${FSAL_DESTINATION} )

########### install files ###############

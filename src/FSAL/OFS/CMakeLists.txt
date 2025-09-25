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

# Optional libhdfs support for real Ozone implementation
option(USE_LIBHDFS "Enable libhdfs support for Ozone integration" OFF)

if(USE_LIBHDFS)
  find_path(HDFS_INCLUDE_DIR hdfs.h
    HINTS 
      ENV HADOOP_HOME
      ENV JAVA_HOME
    PATH_SUFFIXES 
      include
      hadoop/include
      include/hdfs
      share/hadoop/common/lib/native/include
  )
  
  find_library(HDFS_LIBRARY
    NAMES hdfs libhdfs
    HINTS
      ENV HADOOP_HOME
      ENV JAVA_HOME
    PATH_SUFFIXES 
      lib
      lib/native
      hadoop/lib/native
      share/hadoop/common/lib/native
  )
  
  if(HDFS_INCLUDE_DIR AND HDFS_LIBRARY)
    message(STATUS "Found libhdfs: ${HDFS_LIBRARY}")
    message(STATUS "Found hdfs.h: ${HDFS_INCLUDE_DIR}")
    add_definitions(-DUSE_LIBHDFS)
    set(HDFS_FOUND TRUE)
  else()
    message(WARNING "libhdfs requested but not found. Falling back to mock implementation.")
    message(STATUS "  Set HADOOP_HOME or provide -DHDFS_INCLUDE_DIR and -DHDFS_LIBRARY")
    set(HDFS_FOUND FALSE)
  endif()
endif(USE_LIBHDFS)

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

# Add hdfs include directory and library if available
if(HDFS_FOUND)
  target_include_directories(fsalofs PRIVATE ${HDFS_INCLUDE_DIR})
  target_link_libraries(fsalofs ${HDFS_LIBRARY})
endif()

target_link_libraries(fsalofs
  ganesha_nfsd
  ${SYSTEM_LIBRARIES}
  ${LTTNG_LIBRARIES}
  ${LDFLAG_DISALLOW_UNDEF}
)

set_target_properties(fsalofs PROPERTIES VERSION 4.2.0 SOVERSION 4)
install(TARGETS fsalofs COMPONENT fsal DESTINATION ${FSAL_DESTINATION} )

########### install files ###############

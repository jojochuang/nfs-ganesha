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

# libhdfs is required for OFS FSAL - no mock mode
message(STATUS "OFS FSAL: Checking for HDFS dependencies...")

# Only search if not manually specified
if(NOT HDFS_INCLUDE_DIR)
  message(STATUS "OFS FSAL: Searching for hdfs.h...")
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
else()
  message(STATUS "OFS FSAL: Using manually specified HDFS_INCLUDE_DIR: ${HDFS_INCLUDE_DIR}")
endif()

if(NOT HDFS_LIBRARY)
  message(STATUS "OFS FSAL: Searching for libhdfs...")
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
else()
  message(STATUS "OFS FSAL: Using manually specified HDFS_LIBRARY: ${HDFS_LIBRARY}")
endif()

# Debug: Show what we found or didn't find
message(STATUS "OFS FSAL: HDFS_INCLUDE_DIR = ${HDFS_INCLUDE_DIR}")
message(STATUS "OFS FSAL: HDFS_LIBRARY = ${HDFS_LIBRARY}")

if(HDFS_INCLUDE_DIR AND HDFS_LIBRARY)
  message(STATUS "Found libhdfs: ${HDFS_LIBRARY}")
  message(STATUS "Found hdfs.h: ${HDFS_INCLUDE_DIR}")
  add_definitions(-DUSE_LIBHDFS)
  set(HDFS_FOUND TRUE)
else()
  message(FATAL_ERROR "OFS FSAL requires libhdfs but it was not found. "
    "Please install Apache Hadoop and set HADOOP_HOME environment variable, "
    "or manually specify -DHDFS_INCLUDE_DIR and -DHDFS_LIBRARY. "
    "See README.md for installation instructions.")
  set(HDFS_FOUND FALSE)
endif()

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

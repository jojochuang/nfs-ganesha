# FSAL_OFS - Object File System FSAL

## Overview

FSAL_OFS is a skeleton implementation of a File System Abstraction Layer (FSAL) for NFS-Ganesha. This is a minimal working implementation that serves as a starting point for developing object-based filesystem integration.

## Current Status

This is a **skeleton implementation** that provides:

- Basic FSAL registration and initialization
- Minimal module structure following NFS-Ganesha patterns
- Stub functions that return appropriate error codes
- CMake build integration

## Files

- `fsal_ofs_init.c` - Module initialization and export creation
- `fsal_ofs_export.c` - Export operations and management  
- `fsal_ofs_internal.h` - Internal data structures and prototypes
- `fsal_ofs_ops.c` - File operations (stub implementations)  
- `fsal_ofs_handle.c` - Handle operations (stub implementations)
- `fsal_ofs_conf.c` - Configuration handling (stub implementations)
- `fsal_ofs_util.c` - Utility functions (stub implementations)
- `CMakeLists.txt` - Build configuration

## Build Instructions

To build with OFS FSAL support:

```bash
cmake -DUSE_FSAL_OFS=ON <source_directory>
make
```

### Building with Real Ozone/libhdfs Support

The OFS FSAL supports two modes of operation:

1. **Mock Mode** (default) - Uses stub implementations for testing and development
2. **Real Mode** - Uses Apache Hadoop libhdfs to connect to Apache Ozone clusters

To build with real Ozone support via libhdfs:

```bash
# Install Hadoop/libhdfs first (see prerequisites below)
cmake -DUSE_FSAL_OFS=ON -DUSE_LIBHDFS=ON <source_directory>
make
```

#### Prerequisites for Real Ozone Support

To use the real Ozone implementation, you need:

1. **Apache Hadoop** with native libraries installed
2. **Java Development Kit (JDK)** 8 or later
3. **libhdfs** native library

Example installation on Ubuntu/Debian:
```bash
# Install OpenJDK
sudo apt-get install openjdk-8-jdk

# Download and install Hadoop (adjust version as needed)
wget https://archive.apache.org/dist/hadoop/common/hadoop-3.3.4/hadoop-3.3.4.tar.gz
tar -xzf hadoop-3.3.4.tar.gz
export HADOOP_HOME=/path/to/hadoop-3.3.4
export JAVA_HOME=/usr/lib/jvm/java-8-openjdk-amd64

# Ensure libhdfs is available
export LD_LIBRARY_PATH=$HADOOP_HOME/lib/native:$LD_LIBRARY_PATH
```

The build system will automatically detect libhdfs if:
- `HADOOP_HOME` environment variable points to your Hadoop installation
- OR you manually specify: `-DHDFS_INCLUDE_DIR=/path/to/hdfs/include -DHDFS_LIBRARY=/path/to/libhdfs.so`

If libhdfs is not found, the build will automatically fall back to mock mode with a warning.

## Configuration

The OFS FSAL supports comprehensive configuration through an `OFS` block in ganesha.conf:

```
OFS {
    # Ozone URI specifying default volume and bucket
    OzoneURI = "ozone://localhost:9862/vol1/bucket1";
    
    # Whitelist of allowed volumes (comma-separated)
    VolumeWhitelist = "vol1, vol2, vol3";
    
    # Directory for staging files during operations
    StagingDir = "/var/cache/nfs-ganesha-ofs";
    
    # Maximum bytes to use for staging (1GB default)
    MaxStagingBytes = 1073741824;
    
    # Read-ahead buffer size in KB (1MB default)
    ReadAheadKB = 1024;
}
```

### Configuration Parameters

- **OzoneURI**: Optional. Format `[ozone://host:port/]volume/bucket`. Specifies default Ozone service and container.
- **VolumeWhitelist**: Optional. Comma-separated list of allowed volumes. If not set, all volumes are accessible.
- **StagingDir**: Optional. Directory for temporary file staging. Default: `/tmp/nfs-ganesha-ofs`
- **MaxStagingBytes**: Optional. Maximum staging space in bytes. Default: 1GB
- **ReadAheadKB**: Optional. Read-ahead buffer size in KB. Default: 1MB

The configuration parser validates all parameters and fails fast with clear error messages for invalid configs. Effective configuration is logged at INFO level on startup.

To use the OFS FSAL in an export:

```
EXPORT {
    Export_Id = 101;
    Path = "/ozone";
    Pseudo = "/ozone";
    FSAL {
        Name = "OFS";
    }
}
```

**Note**: Basic export creation functionality is now implemented. The OFS FSAL can successfully create exports and will no longer fail with ERR_FSAL_NOTSUPP. However, actual file operations are still not implemented and will return appropriate error codes.

## Development

This implementation provides a solid foundation for a complete OFS FSAL with real Ozone integration. Progress:

1. ✅ Export creation and management - **IMPLEMENTED** 
2. ✅ Configuration parameter handling - **IMPLEMENTED**
3. ✅ Ozone client API with libhdfs integration - **IMPLEMENTED**
4. ⏳ File and directory operations - **IN PROGRESS**
5. ⏳ Handle management - **IN PROGRESS**  
6. ⏳ Complete object store integration - **PLANNED**

### Ozone Client API

The core Ozone client is now fully implemented with:
- Real libhdfs-based implementation for production Ozone clusters
- Mock fallback for development without Hadoop dependencies
- Proper connection management and error handling
- Metadata extraction using hdfsGetPathInfo
- Support for o3fs:// URI scheme

### Implementation Notes

The real Ozone implementation uses:
- **libhdfs** C API as the underlying transport
- **o3fs** (Ozone FileSystem) scheme for path construction
- **hdfsGetPathInfo** for metadata operations (stat/head)
- **hdfsDisconnect** for proper cleanup

Path mapping:
- Ozone `volume/bucket/key` → o3fs `o3fs://bucket.volume.service/key`
- Service URIs support both `ozone://host:port` and plain `host:port` formats
- Automatic slash handling for proper path construction

### Configuration System

The OFS FSAL now includes a complete configuration system:

- `struct ofs_conf` - Configuration parameter structure  
- Configuration parsing and validation with proper error handling
- Support for OzoneURI, VolumeWhitelist, StagingDir, MaxStagingBytes, ReadAheadKB
- Comprehensive unit tests for configuration parsing
- Configuration logging at INFO level on startup

## Current Implementation

### Ozone Client API

The OFS FSAL now implements a complete Ozone client API with both mock and real modes:

#### Functions Implemented:
- ✅ `ofs_ozone_connect()` - Connect to Ozone service via libhdfs/o3fs
- ✅ `ofs_ozone_get_volume()` - Get volume handle with o3fs path construction  
- ✅ `ofs_ozone_get_bucket()` - Get bucket handle with o3fs path construction
- ✅ `ofs_ozone_head_key()` - Head (stat) operation using hdfsGetPathInfo
- ✅ `ofs_ozone_disconnect()` - Proper cleanup of HDFS connections

#### Real Implementation (libhdfs mode):
- Uses Apache Hadoop's libhdfs C API to connect to Ozone clusters
- Constructs o3fs:// URIs (e.g., `o3fs://bucket.volume.service/path`) 
- Maps Ozone operations to HDFS filesystem operations
- Supports metadata extraction (size, mtime, mode, replication, block size)
- Proper error handling and connection management

#### Mock Implementation (fallback mode):
- Provides compatible API for testing without libhdfs dependency
- Simulates basic file/directory operations for development
- Returns realistic metadata for test files and root directory

### Configuration Integration

The Ozone client integrates with the configuration system:
- OzoneURI parameter used for service connection  
- Volume whitelist validation before volume access
- Service URI parsing supports both `ozone://host:port` and `host:port` formats

### Export Creation

The OFS FSAL now implements basic export creation functionality:

- `ofs_create_export()` - Creates and initializes export structures
- `ofs_export_ops_init()` - Initializes export operation vectors
- Configuration parameter parsing for FSAL blocks

### Files

- `fsal_ofs_init.c` - Module initialization and export creation
- `fsal_ofs_export.c` - Export operations and management
- `fsal_ofs_internal.h` - Internal data structures and prototypes
- `fsal_ofs_ops.c` - File operations (stub implementations)  
- `fsal_ofs_handle.c` - Handle operations (stub implementations)
- `fsal_ofs_conf.c` - Configuration handling (**IMPLEMENTED**)
- `fsal_ofs_util.c` - **Ozone client API with libhdfs integration** (**IMPLEMENTED**)
- `CMakeLists.txt` - Build configuration with libhdfs detection

## License

LGPL-3.0-or-later
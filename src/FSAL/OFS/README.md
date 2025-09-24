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

This skeleton provides the foundation for implementing a complete OFS FSAL. Key areas for development:

1. ✅ Export creation and management - **IMPLEMENTED** 
2. ✅ Configuration parameter handling - **IMPLEMENTED**
3. File and directory operations
4. Handle management
5. Object store integration

### Configuration System

The OFS FSAL now includes a complete configuration system:

- `struct ofs_conf` - Configuration parameter structure  
- Configuration parsing and validation with proper error handling
- Support for OzoneURI, VolumeWhitelist, StagingDir, MaxStagingBytes, ReadAheadKB
- Comprehensive unit tests for configuration parsing
- Configuration logging at INFO level on startup

## Current Implementation

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
- `fsal_ofs_util.c` - Utility functions (stub implementations)
- `CMakeLists.txt` - Build configuration

## License

LGPL-3.0-or-later
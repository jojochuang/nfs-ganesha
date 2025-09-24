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

To use the OFS FSAL in your ganesha configuration:

```
FSAL {
    Name = OFS;
}
```

**Note**: Basic export creation functionality is now implemented. The OFS FSAL can successfully create exports and will no longer fail with ERR_FSAL_NOTSUPP. However, actual file operations are still not implemented and will return appropriate error codes.

## Development

This skeleton provides the foundation for implementing a complete OFS FSAL. Key areas for development:

1. ✅ Export creation and management - **IMPLEMENTED** 
2. File and directory operations
3. Handle management
4. Configuration parameter handling
5. Object store integration

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
- `fsal_ofs_conf.c` - Configuration handling (stub implementations)
- `fsal_ofs_util.c` - Utility functions (stub implementations)
- `CMakeLists.txt` - Build configuration

## License

LGPL-3.0-or-later
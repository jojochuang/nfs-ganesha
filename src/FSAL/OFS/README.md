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

- `fsal_ofs_init.c` - Module initialization and registration
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

**Note**: Currently, no exports are supported as this is a skeleton implementation.

## Development

This skeleton provides the foundation for implementing a complete OFS FSAL. Key areas for development:

1. Export creation and management
2. File and directory operations
3. Handle management
4. Configuration parameter handling
5. Object store integration

## License

LGPL-3.0-or-later
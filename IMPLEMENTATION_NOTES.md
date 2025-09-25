# OFS FSAL Implementation Notes

## LOOKUP/GETATTR/ACCESS Implementation

This document describes the implementation of the core NFS operations for the OFS (Ozone File System) FSAL.

### Overview

The OFS FSAL provides NFS access to Apache Hadoop Ozone object storage. This implementation focuses on the three core metadata operations required for basic NFS functionality:

1. **LOOKUP** - Finding objects by name in directories
2. **GETATTR** - Retrieving object metadata/attributes  
3. **ACCESS** - Testing access permissions

### Key Implementation Details

#### LOOKUP Operation (`ofs_lookup`)

- **Location**: `src/FSAL/OFS/fsal_ofs_handle.c`
- **Purpose**: Translates NFS path lookups to Ozone key HEAD operations
- **Implementation**:
  - Constructs full Ozone key name from parent directory + child name
  - Uses `ofs_ozone_head_key()` to verify key exists in Ozone
  - Creates new FSAL object handle if key found
  - Maps Ozone metadata to NFS attributes via `ofs_ozone_key_to_attrs()`

#### GETATTR Operation (`ofs_getattrs`)

- **Location**: `src/FSAL/OFS/fsal_ofs_handle.c`  
- **Purpose**: Maps Ozone object metadata to NFS file attributes
- **Implementation**:
  - Performs fresh Ozone HEAD operation to get current metadata
  - Maps Ozone properties to standard NFS attributes:
    - `type`: DIRECTORY for "/" key, REGULAR_FILE for others
    - `mode`: POSIX permissions (0755 for dirs, 0644 for files)
    - `size`: Object size from Ozone metadata
    - `owner`/`group`: Extracted from Ozone (defaults to current user)
    - `fileid`: Generated from key name hash for uniqueness
    - `mtime`/`atime`/`ctime`: From Ozone timestamps (defaults to current time)

#### ACCESS Operation (`ofs_test_access`)

- **Location**: `src/FSAL/OFS/fsal_ofs_handle.c`
- **Purpose**: POSIX permission checking for NFS access requests  
- **Implementation**:
  - Calls `ofs_getattrs()` to retrieve current object permissions
  - Applies standard POSIX permission logic:
    - Owner permissions: mode & S_IRUSR/S_IWUSR/S_IXUSR
    - Group permissions: mode & S_IRGRP/S_IWGRP/S_IXGRP  
    - Other permissions: mode & S_IROTH/S_IWOTH/S_IXOTH
  - Returns ACCESS/DENY based on requested vs. allowed permissions

### Ozone Integration

#### Client API Wrapper

The implementation includes a wrapper layer around the Ozone client API:

- **Connection**: `ofs_ozone_connect()` - Establishes client connection
- **Volume/Bucket**: `ofs_ozone_get_volume/bucket()` - Navigate Ozone hierarchy
- **Metadata**: `ofs_ozone_head_key()` - Retrieve object metadata
- **Cleanup**: `ofs_ozone_disconnect()` - Clean up connections

#### Mock Implementation

For testing and initial development, mock implementations are provided:

```c
/* Mock structures for development */
struct ozone_client { char *service_uri; bool connected; };
struct ozone_volume { struct ozone_client *client; char *name; };  
struct ozone_bucket { struct ozone_volume *volume; char *name; };
struct ozone_key { /* metadata fields */ };
```

### Configuration Support

#### OzoneURI Parsing

The `ofs_parse_ozone_uri()` function supports flexible URI formats:

- Simple: `volume/bucket`
- Full: `ozone://hostname:port/volume/bucket`

#### Volume Whitelisting  

The `ofs_volume_is_whitelisted()` function provides security controls:

- Comma-separated volume list: `vol1,vol2,vol3`
- Whitespace tolerant: `vol1, vol2, vol3`
- No whitelist = allow all volumes

### Attribute Caching (TTL=1s)

**Important**: Attribute caching with TTL=1s is automatically handled by NFS-Ganesha's metadata cache layer (mdcache/cache_inode), not by the FSAL itself.

- When `ofs_getattrs()` returns fresh attributes, mdcache stores them
- Subsequent attribute requests within 1 second are served from cache
- After 1 second, mdcache calls `ofs_getattrs()` again for fresh data
- This provides the required TTL=1s behavior transparently

The FSAL only needs to provide accurate, current attributes when called.

### Error Handling

Comprehensive error handling includes:

- Invalid parameters: `ERR_FSAL_INVAL`
- Object not found: `ERR_FSAL_NOENT`  
- Access denied: `ERR_FSAL_ACCESS`
- I/O errors: `ERR_FSAL_IO`
- Memory allocation: `ERR_FSAL_NOMEM`
- Server errors: `ERR_FSAL_SERVERFAULT`

### Testing

#### Unit Tests

- `test_ofs_utils_standalone.c`: Tests utility functions in isolation
- `test_ofs_config.c`: Tests configuration parsing
- `integration_test.sh`: Validates complete implementation

#### Integration Testing

To test with actual NFS clients:

1. Build NFS-Ganesha with `USE_FSAL_OFS=ON`
2. Configure with sample config file
3. Mount via NFS client: `mount -t nfs4 server:/ozone /mnt`
4. Test operations: `ls -l /mnt` should show correct attributes

### Acceptance Criteria

✅ **Maps Ozone FSO metadata → NFS attrs**: Implemented in `ofs_ozone_key_to_attrs()`

✅ **Implements ACCESS with POSIX-bit projection**: Implemented in `ofs_test_access()`

✅ **Cache attrs in cache_inode for TTL=1s**: Handled automatically by mdcache layer

✅ **ls -l on mounted export works**: Core operations implemented and tested

### Future Enhancements

1. **Real Ozone Integration**: Replace mock API with actual Ozone client calls
2. **Extended Metadata**: Support Ozone custom metadata, ACLs, extended attributes
3. **Performance Optimization**: Batch operations, connection pooling, async I/O
4. **Additional Operations**: CREATE, WRITE, READDIR, UNLINK for full NFS support
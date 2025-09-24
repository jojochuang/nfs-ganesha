# Copilot Instructions for NFS-Ganesha

## Project Overview

NFS-Ganesha is a user-space NFSv3, NFSv4, and NFSv4.1 fileserver for UNIX/Linux systems. It also supports the 9p.2000L protocol. This is a C/C++ project with a CMake build system.

## Code Style and Standards

### General Guidelines
- Follow the project's existing C coding style conventions
- Use the provided `.clang-format` configuration for code formatting
- Use tabs for indentation (tab width: 8 spaces)
- Keep lines under 80 characters when possible
- Use Linux kernel-style function brace placement (opening brace on new line)

### Naming Conventions
- Use snake_case for function and variable names
- Use ALL_CAPS for constants and macros
- Prefix with module/component name where appropriate
- Follow existing patterns in the codebase

### Code Organization
- Place headers in `src/include/`
- Organize code by functional modules (FSAL, RPCAL, SAL, etc.)
- Use consistent header guard patterns
- Include proper SPDX license headers in new files

## Project Structure

```
src/
├── FSAL/          # File System Abstraction Layer
├── MainNFSD/      # Main NFS daemon code
├── Protocols/     # Protocol implementations (NFS, 9P)
├── RPCAL/         # RPC Abstraction Layer
├── SAL/           # State Abstraction Layer
├── config_parsing/ # Configuration file parsing
├── hashtable/     # Hash table implementations
├── include/       # Header files
├── log/           # Logging subsystem
├── scripts/       # Utility scripts and tools
├── support/       # Support libraries
└── test/          # Test code
```

## Build System

### CMake Configuration
- Primary build system is CMake
- Build out-of-source in a separate directory
- Use existing CMakeLists.txt patterns for new components
- Support both system-provided and bundled libntirpc

### Common Build Commands
```bash
# Clone with submodules
git clone --recursive https://github.com/nfs-ganesha/nfs-ganesha.git

# Update submodules if needed
git submodule update --init

# Build (out-of-source)
mkdir build && cd build
cmake ../src
make

# Alternative build location
cmake -DCMAKE_INSTALL_PREFIX=/usr/local ../src
```

### Build Options
- Use existing CMake options for features (USE_FSAL_*, USE_DBUS, etc.)
- Check existing CMakeLists.txt for available build flags
- Maintain compatibility with existing build configurations

## Development Workflow

### Git and Contribution Process
- **Important**: This project uses Gerrithub for code review, NOT GitHub pull requests
- Install git hooks before making changes: `src/scripts/git_hooks/install_git_hooks.sh`
- Follow the process outlined in `src/CONTRIBUTING_HOWTO.txt`
- Maintain proper commit message format with Change-Id
- Use Gerrithub for patch submission and review

### Testing
- Run existing tests in `src/test/`
- Use `src/test/run_test_mode.sh` for basic testing
- Consider pjdfstest for file system testing
- Test with different FSAL backends when applicable

## Common Tasks

### Adding New Features
1. Follow existing module patterns
2. Add appropriate CMake build rules
3. Include proper error handling and logging
4. Add documentation where needed
5. Consider compatibility with existing FSALs

### Bug Fixes
1. Identify the affected component(s)
2. Check for similar issues in other modules
3. Test across different FSAL backends if relevant
4. Ensure fix doesn't break existing functionality

### Configuration Changes
- Configuration parsing is in `src/config_parsing/`
- Follow existing parameter patterns
- Update documentation for new config options
- Consider backward compatibility

## Dependencies and Libraries

### Core Dependencies
- libntirpc (bundled as submodule or system-provided)
- Standard C/POSIX libraries
- CMake for building

### Optional Dependencies
- D-Bus (for management interface)
- Various storage backend libraries (GPFS, Ceph, etc.)
- GUI tools dependencies (Qt for admin tools)

### FSAL-Specific Dependencies
- Different FSALs may have specific library requirements
- Check individual FSAL directories for requirements

## Debugging and Logging

- Use the project's logging framework in `src/log/`
- Follow existing log level conventions (DEBUG, INFO, WARN, ERROR, FATAL)
- Include contextual information in log messages
- Use appropriate log categories for different components

## File System Abstraction Layer (FSAL)

- FSAL provides abstraction for different storage backends
- Common FSALs: VFS, Proxy, GPFS, Ceph, etc.
- Follow existing FSAL patterns when adding new backends
- Implement required FSAL operations consistently

## Security Considerations

- Handle authentication and authorization properly
- Validate input parameters thoroughly
- Use secure coding practices (bounds checking, etc.)
- Consider NFSv4 security features and Kerberos integration

## Performance Guidelines

- Consider multi-threading implications
- Use efficient data structures (existing hashtables, etc.)
- Profile critical paths when making changes
- Maintain scalability for large deployments

## Documentation

- Update relevant documentation when making changes
- Check `src/doc/` for existing documentation
- Update man pages if adding new features or options
- Keep README and HOWTO files current

## Compatibility Notes

- Maintain compatibility with existing NFS clients
- Consider impact on different operating systems
- Test with various FSAL backends
- Maintain API/ABI compatibility where possible
"""
Common compiler options for C projects.
Centralized flag management for consistency across the entire codebase.
"""

# Base C compilation options for all builds
C_COPTS = [
    "-Wall",
    "-Wextra",
    "-pedantic",
    "-Wshadow",
    "-Wconversion",
    "-Wno-unused-parameter",
    "-Werror=implicit-function-declaration",
    "-std=c11",
]

# Debug-specific compiler options
C_DEBUG_COPTS = C_COPTS + [
    "-g",
    "-O0",
    "-fno-omit-frame-pointer",
    "-DDEBUG",
]

# Release/Optimized compiler options
C_RELEASE_COPTS = C_COPTS + [
    "-O3",
    "-march=native",
    "-DNDEBUG",
]

# Linker options
C_LINKOPTS = [
    "-lm",  # Link math library
]

# Platform-specific options
C_MACOS_COPTS = []
C_LINUX_COPTS = []
C_WINDOWS_COPTS = []

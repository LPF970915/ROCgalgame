set(VCPKG_TARGET_ARCHITECTURE arm64)
set(VCPKG_CRT_LINKAGE dynamic)
set(VCPKG_LIBRARY_LINKAGE static)
set(VCPKG_CMAKE_SYSTEM_NAME Linux)
set(VCPKG_CHAINLOAD_TOOLCHAIN_FILE
    "${CMAKE_CURRENT_LIST_DIR}/../toolchain/aarch64-gkd-krkr2.cmake")

# The outer build passes the GKD toolchain through
# VCPKG_CHAINLOAD_TOOLCHAIN_FILE so ports use the same glibc sysroot.
set(VCPKG_ENV_PASSTHROUGH_UNTRACKED GKD_SYSROOT)

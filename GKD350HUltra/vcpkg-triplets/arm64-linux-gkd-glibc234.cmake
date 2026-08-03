set(VCPKG_TARGET_ARCHITECTURE arm64)
set(VCPKG_CRT_LINKAGE dynamic)
set(VCPKG_LIBRARY_LINKAGE static)
set(VCPKG_BUILD_TYPE release)
set(VCPKG_CMAKE_SYSTEM_NAME Linux)
set(VCPKG_CHAINLOAD_TOOLCHAIN_FILE
    "${CMAKE_CURRENT_LIST_DIR}/../toolchain/aarch64-gkd-krkr2.cmake")

# Keep old device-sysroot artifacts out of the glibc 2.34 build cache.
set(VCPKG_ENV_PASSTHROUGH GKD_SYSROOT ROCGALGAME_GLIBC_BASELINE)

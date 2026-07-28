set(COCOS2D_VERSION cocos2d-x-${VERSION})

# KRKR2 is packaged as a Release binary; avoid building an unused debug copy
# of the large Cocos2d-x static library on the handheld cross toolchain.
set(VCPKG_BUILD_TYPE release)

set(VCPKG_POLICY_ALLOW_RESTRICTED_HEADERS enabled)
set(VCPKG_POLICY_ALLOW_EMPTY_FOLDERS enabled)

vcpkg_download_distfile(
    ARCHIVE
    URLS https://github.com/cocos2d/cocos2d-x/archive/refs/tags/${COCOS2D_VERSION}.tar.gz
    FILENAME ${COCOS2D_VERSION}.tar.gz
    SHA512 b2d5ac968231892c39a953d82e9791c2182b0dbceca5791647bb2daad258134725386c9eb1d32de148465d88d2d932b29f241af0f5f4b4e6d9d80d9684f531fa
)

if(VCPKG_TARGET_IS_LINUX)
    message(WARNING "${PORT} currently requires external library from the system package manager:
    On Ubuntu derivatives:
        sudo apt install libxxf86vm-dev libx11-dev libxmu-dev libglu1-mesa-dev libgl2ps-dev libxi-dev libzip-dev libpng-dev libcurl4-gnutls-dev libfontconfig1-dev libsqlite3-dev libglew-dev libssl-dev libgtk-3-dev binutils")

endif()

vcpkg_extract_source_archive(
    SOURCE_PATH
    ARCHIVE "${ARCHIVE}"
    SOURCE_BASE "${COCOS2D_VERSION}"
    PATCHES
        patch/0001-add-cstdint-header.patch
        patch/fix-iconv-cast.patch
        patch/fix-mac-audio-build.patch
        patch/fix-mac-glew.patch
        patch/fix-mac-glfw3.patch
        patch/fix-unzip.patch
        patch/fix-chipmunk-Hasty.patch
        patch/fix-win64.patch
        patch/fix-bullet-spell.patch
        patch/fix-chipmunk.patch
        patch/linux-wayland-gles2.patch
        patch/linux-wayland-swap-damage.patch
)

file(COPY "${CMAKE_CURRENT_LIST_DIR}/cocos2dx-config.cmake.in" DESTINATION "${SOURCE_PATH}")

file(COPY_FILE "${CMAKE_CURRENT_LIST_DIR}/patch/cocos2d-x/CMakeLists.txt" "${SOURCE_PATH}/CMakeLists.txt" ONLY_IF_DIFFERENT)
file(COPY_FILE "${CMAKE_CURRENT_LIST_DIR}/patch/cocos2d-x/cocos/CMakeLists.txt" "${SOURCE_PATH}/cocos/CMakeLists.txt" ONLY_IF_DIFFERENT)

file(COPY_FILE "${CMAKE_CURRENT_LIST_DIR}/patch/cocos2d-x/cmake/Modules/CocosBuildHelpers.cmake" "${SOURCE_PATH}/cmake/Modules/CocosBuildHelpers.cmake" ONLY_IF_DIFFERENT)
file(COPY_FILE "${CMAKE_CURRENT_LIST_DIR}/patch/cocos2d-x/cmake/Modules/CocosConfigDepend.cmake" "${SOURCE_PATH}/cmake/Modules/CocosConfigDepend.cmake" ONLY_IF_DIFFERENT)

include("${CMAKE_CURRENT_LIST_DIR}/DownloadDeps.cmake")

file(COPY_FILE "${CMAKE_CURRENT_LIST_DIR}/patch/cocos2d-x/external/CMakeLists.txt" "${SOURCE_PATH}/external/CMakeLists.txt" ONLY_IF_DIFFERENT)

set(GKD_SAVED_PKG_CONFIG_SYSROOT_DIR "$ENV{PKG_CONFIG_SYSROOT_DIR}")
set(ENV{PKG_CONFIG_SYSROOT_DIR} "")

if(_VCPKG_EDITABLE AND EXISTS "${CURRENT_BUILDTREES_DIR}/${TARGET_TRIPLET}-rel/build.ninja")
    message(STATUS "Reusing existing editable Cocos2d-x buildtree")
else()
    vcpkg_cmake_configure(
        SOURCE_PATH "${SOURCE_PATH}"
        OPTIONS
            -DBUILD_TESTS=OFF
            -DBUILD_JS_LIBS=OFF
            -DBUILD_LUA_LIBS=OFF
            -DROCGALGAME_LINUX_GLES2=ON
    )
endif()

set(ENV{PKG_CONFIG_SYSROOT_DIR} "${GKD_SAVED_PKG_CONFIG_SYSROOT_DIR}")

vcpkg_cmake_install()

vcpkg_copy_pdbs()

vcpkg_cmake_config_fixup()

file(REMOVE_RECURSE "${CURRENT_PACKAGES_DIR}/debug/share")
file(REMOVE_RECURSE "${CURRENT_PACKAGES_DIR}/debug/include")

file(GLOB LICENSE_FILES "${SOURCE_PATH}/licenses/*")
vcpkg_install_copyright(FILE_LIST ${LICENSE_FILES})

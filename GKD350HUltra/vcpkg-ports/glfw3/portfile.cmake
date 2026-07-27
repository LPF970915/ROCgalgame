vcpkg_from_github(
    OUT_SOURCE_PATH SOURCE_PATH
    REPO glfw/glfw
    REF ${VERSION}
    SHA512 39ad7a4521267fbebc35d2ff0c389a56236ead5fa4bdff33db113bd302f70f5f2869ff4e6db1979512e1542813292dff5a482e94dfce231750f0746c301ae9ed
    HEAD_REF master
)

vcpkg_check_features(OUT_FEATURE_OPTIONS FEATURE_OPTIONS
    FEATURES
    wayland GLFW_BUILD_WAYLAND
)

set(GKD_SAVED_PKG_CONFIG_SYSROOT_DIR "$ENV{PKG_CONFIG_SYSROOT_DIR}")
set(GKD_SAVED_PKG_CONFIG_LIBDIR "$ENV{PKG_CONFIG_LIBDIR}")
if(VCPKG_TARGET_IS_LINUX AND "wayland" IN_LIST FEATURES)
    if(NOT DEFINED ENV{GKD_SYSROOT} OR "$ENV{GKD_SYSROOT}" STREQUAL "")
        message(FATAL_ERROR "GKD_SYSROOT is required for the GLFW Wayland feature")
    endif()
    get_filename_component(GKD_PORT_ROOT "${CMAKE_CURRENT_LIST_DIR}/../.." ABSOLUTE)
    set(ENV{PKG_CONFIG_SYSROOT_DIR} "$ENV{GKD_SYSROOT}")
    set(ENV{PKG_CONFIG_LIBDIR} "${GKD_PORT_ROOT}/pkgconfig-wayland")

    # The chainloaded cross toolchain intentionally resets pkg-config's search
    # path.  Use a port-local wrapper so only GLFW sees the target Wayland
    # metadata and the rest of the vcpkg dependency graph keeps its ABI.
    set(GKD_PKG_CONFIG_WRAPPER "${CURRENT_BUILDTREES_DIR}/gkd-wayland-pkg-config")
    file(WRITE "${GKD_PKG_CONFIG_WRAPPER}"
        "#!/bin/sh\n"
        "export PKG_CONFIG_SYSROOT_DIR=\"$ENV{GKD_SYSROOT}\"\n"
        "export PKG_CONFIG_LIBDIR=\"${GKD_PORT_ROOT}/pkgconfig-wayland\"\n"
        "exec /usr/bin/pkg-config \"$@\"\n")
    file(CHMOD "${GKD_PKG_CONFIG_WRAPPER}"
        PERMISSIONS OWNER_READ OWNER_WRITE OWNER_EXECUTE GROUP_READ GROUP_EXECUTE WORLD_READ WORLD_EXECUTE)
endif()

vcpkg_cmake_configure(
    SOURCE_PATH "${SOURCE_PATH}"
    OPTIONS
        -DGLFW_BUILD_EXAMPLES=OFF
        -DGLFW_BUILD_TESTS=OFF
        -DGLFW_BUILD_DOCS=OFF
        -DGLFW_BUILD_X11=OFF
        -DPKG_CONFIG_EXECUTABLE=${GKD_PKG_CONFIG_WRAPPER}
        ${FEATURE_OPTIONS}
)

set(ENV{PKG_CONFIG_SYSROOT_DIR} "${GKD_SAVED_PKG_CONFIG_SYSROOT_DIR}")
set(ENV{PKG_CONFIG_LIBDIR} "${GKD_SAVED_PKG_CONFIG_LIBDIR}")

vcpkg_cmake_install()
vcpkg_cmake_config_fixup(CONFIG_PATH lib/cmake/glfw3)
vcpkg_fixup_pkgconfig()
file(REMOVE_RECURSE "${CURRENT_PACKAGES_DIR}/debug/include")
vcpkg_install_copyright(FILE_LIST "${SOURCE_PATH}/LICENSE.md")

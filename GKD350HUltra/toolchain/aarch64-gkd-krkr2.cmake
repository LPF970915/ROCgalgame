set(CMAKE_SYSTEM_NAME Linux)
set(CMAKE_SYSTEM_PROCESSOR aarch64)

if(NOT DEFINED GKD_SYSROOT AND DEFINED ENV{GKD_SYSROOT})
  set(GKD_SYSROOT "$ENV{GKD_SYSROOT}")
endif()

if(NOT DEFINED GKD_SYSROOT OR GKD_SYSROOT STREQUAL "")
  message(FATAL_ERROR "GKD_SYSROOT is required")
endif()

set(GKD_SYSROOT "${GKD_SYSROOT}" CACHE PATH "GKD350HUltra target sysroot")
list(APPEND CMAKE_TRY_COMPILE_PLATFORM_VARIABLES GKD_SYSROOT)

set(CMAKE_SYSROOT "${GKD_SYSROOT}")
set(CMAKE_C_COMPILER aarch64-linux-gnu-gcc)
set(CMAKE_CXX_COMPILER aarch64-linux-gnu-g++)
set(CMAKE_AR aarch64-linux-gnu-ar)
set(CMAKE_RANLIB aarch64-linux-gnu-ranlib)
set(CMAKE_STRIP aarch64-linux-gnu-strip)

# vcpkg prepends its target install roots after the chainload toolchain runs.
# CMake may load this file again later, so preserve those roots instead of
# replacing them with the device sysroot on the second pass.
list(APPEND CMAKE_FIND_ROOT_PATH "${GKD_SYSROOT}")
list(REMOVE_DUPLICATES CMAKE_FIND_ROOT_PATH)
set(CMAKE_FIND_ROOT_PATH_MODE_PROGRAM NEVER)
set(CMAKE_FIND_ROOT_PATH_MODE_LIBRARY ONLY)
set(CMAKE_FIND_ROOT_PATH_MODE_INCLUDE ONLY)
# vcpkg config packages live outside the target sysroot.
set(CMAKE_FIND_ROOT_PATH_MODE_PACKAGE BOTH)

set(CMAKE_C_FLAGS_INIT "--sysroot=${GKD_SYSROOT}")
set(CMAKE_CXX_FLAGS_INIT "--sysroot=${GKD_SYSROOT}")
set(CMAKE_EXE_LINKER_FLAGS_INIT "--sysroot=${GKD_SYSROOT}")

set(ENV{PKG_CONFIG_SYSROOT_DIR} "${GKD_SYSROOT}")
set(ENV{PKG_CONFIG_LIBDIR}
    "${GKD_SYSROOT}/usr/lib/pkgconfig:${GKD_SYSROOT}/usr/lib/aarch64-linux-gnu/pkgconfig:${GKD_SYSROOT}/usr/share/pkgconfig")

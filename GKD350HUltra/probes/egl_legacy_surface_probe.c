#define _GNU_SOURCE
#include <EGL/egl.h>
#include <dlfcn.h>
#include <stdio.h>
#include <string.h>

typedef __eglMustCastToProperFunctionPointerType (*GetProcAddressFn)(const char*);

static EGLSurface create_platform_window_surface(
    EGLDisplay display, EGLConfig config, void* native_window,
    const EGLint* attributes) {
    fprintf(stderr, "[egl_legacy_surface_probe] using eglCreateWindowSurface\n");
    return eglCreateWindowSurface(
        display, config, (EGLNativeWindowType)native_window, attributes);
}

__eglMustCastToProperFunctionPointerType eglGetProcAddress(const char* name) {
    static GetProcAddressFn real_get_proc_address;
    if (!real_get_proc_address)
        real_get_proc_address = (GetProcAddressFn)dlsym(RTLD_NEXT, "eglGetProcAddress");

    if (name && strcmp(name, "eglCreatePlatformWindowSurfaceEXT") == 0)
        return (__eglMustCastToProperFunctionPointerType)
            create_platform_window_surface;
    return real_get_proc_address ? real_get_proc_address(name) : NULL;
}

#define _GNU_SOURCE
#include <EGL/egl.h>
#include <GLES2/gl2.h>
#include <dlfcn.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>

typedef EGLBoolean (*SwapBuffersFn)(EGLDisplay, EGLSurface);

static EGLConfig find_config(EGLDisplay display, EGLint wanted_id) {
    EGLConfig configs[256];
    EGLint count = 0;
    if(!eglGetConfigs(display, configs, 256, &count)) return NULL;
    for(EGLint i = 0; i < count; ++i) {
        EGLint id = 0;
        if(eglGetConfigAttrib(display, configs[i], EGL_CONFIG_ID, &id) &&
           id == wanted_id) return configs[i];
    }
    return NULL;
}

static EGLint config_attr(EGLDisplay display, EGLConfig config, EGLint attr) {
    EGLint value = -1;
    if(config) eglGetConfigAttrib(display, config, attr, &value);
    return value;
}

static void capture_if_requested(EGLDisplay display, EGLSurface surface,
                                 unsigned int call) {
    const char *request = getenv("ROCGALGAME_EGL_CAPTURE_REQUEST");
    if(!request || !request[0] || access(request, F_OK) != 0) return;
    unlink(request);

    const char *output = getenv("ROCGALGAME_EGL_CAPTURE_OUTPUT");
    if(!output || !output[0]) output = "/tmp/krkr2-egl-capture.ppm";

    EGLint width = 0;
    EGLint height = 0;
    if(!eglQuerySurface(display, surface, EGL_WIDTH, &width) ||
       !eglQuerySurface(display, surface, EGL_HEIGHT, &height) ||
       width <= 0 || height <= 0 || width > 8192 || height > 8192) {
        fprintf(stderr, "[egl_probe] capture failed: invalid surface %dx%d\n",
                width, height);
        return;
    }

    const size_t pixel_count = (size_t)width * (size_t)height;
    uint8_t *rgba = (uint8_t *)malloc(pixel_count * 4);
    uint8_t *rgb_row = (uint8_t *)malloc((size_t)width * 3);
    if(!rgba || !rgb_row) {
        fprintf(stderr, "[egl_probe] capture failed: allocation %dx%d\n",
                width, height);
        free(rgb_row);
        free(rgba);
        return;
    }

    glPixelStorei(GL_PACK_ALIGNMENT, 1);
    glReadPixels(0, 0, width, height, GL_RGBA, GL_UNSIGNED_BYTE, rgba);
    const GLenum read_error = glGetError();
    FILE *file = read_error == GL_NO_ERROR ? fopen(output, "wb") : NULL;
    if(file) {
        fprintf(file, "P6\n%d %d\n255\n", width, height);
        for(EGLint y = height - 1; y >= 0; --y) {
            const uint8_t *source = rgba + (size_t)y * (size_t)width * 4;
            for(EGLint x = 0; x < width; ++x) {
                rgb_row[x * 3] = source[x * 4];
                rgb_row[x * 3 + 1] = source[x * 4 + 1];
                rgb_row[x * 3 + 2] = source[x * 4 + 2];
            }
            fwrite(rgb_row, 3, (size_t)width, file);
        }
        fclose(file);
    }
    fprintf(stderr,
            "[egl_probe] capture call=%u output=%s surface=%dx%d error=0x%04X result=%d\n",
            call, output, width, height, (unsigned int)read_error,
            file ? 1 : 0);
    fflush(stderr);
    free(rgb_row);
    free(rgba);
}

EGLBoolean eglSwapBuffers(EGLDisplay display, EGLSurface surface) {
    static SwapBuffersFn real_swap;
    static unsigned int calls;
    if(!real_swap)
        real_swap = (SwapBuffersFn)dlsym(RTLD_NEXT, "eglSwapBuffers");
    ++calls;
    const int report = calls <= 64 && (calls & (calls - 1)) == 0;
    if(report) {
        EGLint config_id = -1;
        EGLint width = -1, height = -1, swap_behavior = -1;
        EGLint render_buffer = -1;
        const EGLContext context = eglGetCurrentContext();
        eglQueryContext(display, context, EGL_CONFIG_ID, &config_id);
        const EGLConfig config = find_config(display, config_id);
        eglQuerySurface(display, surface, EGL_WIDTH, &width);
        eglQuerySurface(display, surface, EGL_HEIGHT, &height);
        eglQuerySurface(display, surface, EGL_SWAP_BEHAVIOR, &swap_behavior);
        eglQuerySurface(display, surface, EGL_RENDER_BUFFER, &render_buffer);
        GLint framebuffer = -1;
        GLint viewport[4] = {-1, -1, -1, -1};
        glGetIntegerv(GL_FRAMEBUFFER_BINDING, &framebuffer);
        glGetIntegerv(GL_VIEWPORT, viewport);
        fprintf(stderr,
                "[egl_probe] call=%u config=%d rgba=%d/%d/%d/%d "
                "buffer=%d depth=%d stencil=%d native_visual=0x%X "
                "surface_type=0x%X renderable=0x%X surface=%dx%d "
                "swap_behavior=0x%X render_buffer=0x%X fbo=%d "
                "viewport=%d,%d,%d,%d\n",
                calls, config_id,
                config_attr(display, config, EGL_RED_SIZE),
                config_attr(display, config, EGL_GREEN_SIZE),
                config_attr(display, config, EGL_BLUE_SIZE),
                config_attr(display, config, EGL_ALPHA_SIZE),
                config_attr(display, config, EGL_BUFFER_SIZE),
                config_attr(display, config, EGL_DEPTH_SIZE),
                config_attr(display, config, EGL_STENCIL_SIZE),
                config_attr(display, config, EGL_NATIVE_VISUAL_ID),
                config_attr(display, config, EGL_SURFACE_TYPE),
                config_attr(display, config, EGL_RENDERABLE_TYPE),
                width, height, swap_behavior, render_buffer, framebuffer,
                viewport[0], viewport[1], viewport[2], viewport[3]);
        fflush(stderr);
    }
    capture_if_requested(display, surface, calls);
    const EGLBoolean result = real_swap ? real_swap(display, surface) : EGL_FALSE;
    if(report) {
        const EGLint error = eglGetError();
        fprintf(stderr, "[egl_probe] call=%u result=%u error=0x%04X\n",
                calls, (unsigned int)result, (unsigned int)error);
        fflush(stderr);
    }
    return result;
}

#define _GNU_SOURCE
#include <GLES2/gl2.h>
#include <dlfcn.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

typedef struct SDL_Window SDL_Window;
typedef void (*SDLGLSwapWindowFn)(SDL_Window *window);
typedef void (*SDLGLGetDrawableSizeFn)(SDL_Window *window, int *width,
                                      int *height);

static void capture_if_requested(SDL_Window *window,
                                 SDLGLGetDrawableSizeFn get_drawable_size,
                                 unsigned int call) {
    const char *request = getenv("ROCGALGAME_GL_CAPTURE_REQUEST");
    if(!request || !request[0] || access(request, F_OK) != 0) return;
    unlink(request);

    const char *output = getenv("ROCGALGAME_GL_CAPTURE_OUTPUT");
    if(!output || !output[0]) output = "/tmp/krkr2-gl-capture.ppm";

    int width = 0;
    int height = 0;
    if(get_drawable_size) get_drawable_size(window, &width, &height);
    if(width <= 0 || height <= 0 || width > 8192 || height > 8192) {
        fprintf(stderr, "[sdl_gl_capture] invalid drawable %dx%d\n",
                width, height);
        return;
    }

    const size_t pixel_count = (size_t)width * (size_t)height;
    uint8_t *rgba = (uint8_t *)malloc(pixel_count * 4);
    uint8_t *rgb_row = (uint8_t *)malloc((size_t)width * 3);
    if(!rgba || !rgb_row) {
        fprintf(stderr, "[sdl_gl_capture] allocation failed %dx%d\n",
                width, height);
        free(rgb_row);
        free(rgba);
        return;
    }

    while(glGetError() != GL_NO_ERROR) {}
    GLint previous_framebuffer = 0;
    glGetIntegerv(GL_FRAMEBUFFER_BINDING, &previous_framebuffer);
    const char *capture_current = getenv(
        "ROCGALGAME_GL_CAPTURE_CURRENT_FRAMEBUFFER");
    const GLint capture_framebuffer = capture_current &&
        strcmp(capture_current, "1") == 0 ? previous_framebuffer : 0;
    if(capture_framebuffer != previous_framebuffer)
        glBindFramebuffer(GL_FRAMEBUFFER, (GLuint)capture_framebuffer);
    GLint viewport[4] = {0, 0, width, height};
    glGetIntegerv(GL_VIEWPORT, viewport);
    int capture_x = 0;
    int capture_y = 0;
    int capture_width = width;
    int capture_height = height;
    if(capture_framebuffer != 0 && viewport[2] > 0 && viewport[3] > 0 &&
       viewport[2] <= width && viewport[3] <= height) {
        capture_x = viewport[0];
        capture_y = viewport[1];
        capture_width = viewport[2];
        capture_height = viewport[3];
    }
    const GLenum framebuffer_status = glCheckFramebufferStatus(GL_FRAMEBUFFER);
    glPixelStorei(GL_PACK_ALIGNMENT, 1);
    glReadPixels(capture_x, capture_y, capture_width, capture_height,
                 GL_RGBA, GL_UNSIGNED_BYTE, rgba);
    const GLenum error = glGetError();
    if(capture_framebuffer != previous_framebuffer)
        glBindFramebuffer(GL_FRAMEBUFFER, (GLuint)previous_framebuffer);
    FILE *file = error == GL_NO_ERROR ? fopen(output, "wb") : NULL;
    if(file) {
        fprintf(file, "P6\n%d %d\n255\n", capture_width, capture_height);
        for(int output_y = 0; output_y < capture_height; ++output_y) {
            const int source_y = capture_framebuffer == 0
                ? capture_height - 1 - output_y : output_y;
            const uint8_t *source = rgba +
                (size_t)source_y * (size_t)capture_width * 4;
            for(int x = 0; x < capture_width; ++x) {
                rgb_row[x * 3] = source[x * 4];
                rgb_row[x * 3 + 1] = source[x * 4 + 1];
                rgb_row[x * 3 + 2] = source[x * 4 + 2];
            }
            fwrite(rgb_row, 3, (size_t)capture_width, file);
        }
        fclose(file);
    }
    fprintf(stderr,
            "[sdl_gl_capture] call=%u output=%s drawable=%dx%d previous_fbo=%d "
            "capture_fbo=%d viewport=%d,%d %dx%d capture=%dx%d "
            "status=0x%04X error=0x%04X result=%d\n",
            call, output, width, height, previous_framebuffer,
            capture_framebuffer, viewport[0], viewport[1], viewport[2],
            viewport[3], capture_width, capture_height,
            (unsigned int)framebuffer_status, (unsigned int)error,
            file ? 1 : 0);
    fflush(stderr);
    free(rgb_row);
    free(rgba);
}

void SDL_GL_SwapWindow(SDL_Window *window) {
    static SDLGLSwapWindowFn real_swap;
    static SDLGLGetDrawableSizeFn get_drawable_size;
    static unsigned int calls;
    if(!real_swap) {
        real_swap = (SDLGLSwapWindowFn)dlsym(RTLD_NEXT, "SDL_GL_SwapWindow");
        get_drawable_size = (SDLGLGetDrawableSizeFn)dlsym(
            RTLD_NEXT, "SDL_GL_GetDrawableSize");
    }
    ++calls;
    capture_if_requested(window, get_drawable_size, calls);
    if(real_swap) real_swap(window);
}

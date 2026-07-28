#define _GNU_SOURCE
#include <SDL2/SDL.h>
#include <GLES2/gl2.h>
#include <dlfcn.h>
#include <stdio.h>

typedef void (*SwapWindowFn)(SDL_Window*);

void SDL_GL_SwapWindow(SDL_Window* window) {
    static SwapWindowFn real_swap;
    static int reported;
    if (!real_swap)
        real_swap = (SwapWindowFn)dlsym(RTLD_NEXT, "SDL_GL_SwapWindow");

    int width = 0;
    int height = 0;
    SDL_GL_GetDrawableSize(window, &width, &height);
    glBindFramebuffer(GL_FRAMEBUFFER, 0);
    glDisable(GL_SCISSOR_TEST);
    glViewport(0, 0, width, height);
    glClearColor(0.95f, 0.05f, 0.75f, 1.0f);
    glClear(GL_COLOR_BUFFER_BIT);
    glFinish();

    if (!reported) {
        unsigned char pixel[4] = {};
        glReadPixels(width / 2, height / 2, 1, 1, GL_RGBA,
                     GL_UNSIGNED_BYTE, pixel);
        fprintf(stderr,
                "[sdl_force_color] drawable=%dx%d pixel=%u,%u,%u,%u error=0x%04X\n",
                width, height, pixel[0], pixel[1], pixel[2], pixel[3],
                glGetError());
        fflush(stderr);
        reported = 1;
    }

    if (real_swap)
        real_swap(window);
}

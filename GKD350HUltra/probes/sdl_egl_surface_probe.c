#include <SDL2/SDL.h>
#include <EGL/egl.h>
#include <GLES2/gl2.h>
#include <stdio.h>
#include <stdlib.h>

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

static EGLint attr(EGLDisplay display, EGLConfig config, EGLint name) {
    EGLint value = -1;
    if(config) eglGetConfigAttrib(display, config, name, &value);
    return value;
}

int main(void) {
    setenv("SDL_VIDEODRIVER", "wayland", 1);
    SDL_GL_SetAttribute(SDL_GL_CONTEXT_PROFILE_MASK, SDL_GL_CONTEXT_PROFILE_ES);
    SDL_GL_SetAttribute(SDL_GL_CONTEXT_MAJOR_VERSION, 2);
    SDL_GL_SetAttribute(SDL_GL_CONTEXT_MINOR_VERSION, 0);
    SDL_GL_SetAttribute(SDL_GL_RED_SIZE, 8);
    SDL_GL_SetAttribute(SDL_GL_GREEN_SIZE, 8);
    SDL_GL_SetAttribute(SDL_GL_BLUE_SIZE, 8);
    SDL_GL_SetAttribute(SDL_GL_ALPHA_SIZE, 8);
    SDL_GL_SetAttribute(SDL_GL_DEPTH_SIZE, 24);
    SDL_GL_SetAttribute(SDL_GL_STENCIL_SIZE, 8);
    if(SDL_Init(SDL_INIT_VIDEO) != 0) {
        fprintf(stderr, "SDL_Init: %s\n", SDL_GetError());
        return 1;
    }
    SDL_Window *window = SDL_CreateWindow(
        "sdl-egl-surface-probe", SDL_WINDOWPOS_UNDEFINED,
        SDL_WINDOWPOS_UNDEFINED, 1600, 1440,
        SDL_WINDOW_OPENGL | SDL_WINDOW_SHOWN | SDL_WINDOW_FULLSCREEN_DESKTOP);
    if(!window) {
        fprintf(stderr, "SDL_CreateWindow: %s\n", SDL_GetError());
        return 2;
    }
    SDL_GLContext context = SDL_GL_CreateContext(window);
    if(!context) {
        fprintf(stderr, "SDL_GL_CreateContext: %s\n", SDL_GetError());
        return 3;
    }
    SDL_GL_SetSwapInterval(0);
    EGLDisplay display = eglGetCurrentDisplay();
    EGLSurface surface = eglGetCurrentSurface(EGL_DRAW);
    EGLContext egl_context = eglGetCurrentContext();
    EGLint config_id = -1, width = -1, height = -1, swap_behavior = -1;
    eglQueryContext(display, egl_context, EGL_CONFIG_ID, &config_id);
    EGLConfig config = find_config(display, config_id);
    eglQuerySurface(display, surface, EGL_WIDTH, &width);
    eglQuerySurface(display, surface, EGL_HEIGHT, &height);
    eglQuerySurface(display, surface, EGL_SWAP_BEHAVIOR, &swap_behavior);
    fprintf(stderr,
            "config=%d rgba=%d/%d/%d/%d buffer=%d depth=%d stencil=%d "
            "native_visual=0x%X surface_type=0x%X renderable=0x%X "
            "surface=%dx%d swap_behavior=0x%X\n",
            config_id, attr(display, config, EGL_RED_SIZE),
            attr(display, config, EGL_GREEN_SIZE),
            attr(display, config, EGL_BLUE_SIZE),
            attr(display, config, EGL_ALPHA_SIZE),
            attr(display, config, EGL_BUFFER_SIZE),
            attr(display, config, EGL_DEPTH_SIZE),
            attr(display, config, EGL_STENCIL_SIZE),
            attr(display, config, EGL_NATIVE_VISUAL_ID),
            attr(display, config, EGL_SURFACE_TYPE),
            attr(display, config, EGL_RENDERABLE_TYPE), width, height,
            swap_behavior);
    for(int frame = 0; frame < 600; ++frame) {
        const float phase = (frame / 120) % 2 ? 0.15f : 0.85f;
        glViewport(0, 0, 1600, 1440);
        glClearColor(phase, 0.12f, 1.0f - phase, 1.0f);
        glClear(GL_COLOR_BUFFER_BIT);
        SDL_GL_SwapWindow(window);
        SDL_Delay(16);
    }
    SDL_GL_DeleteContext(context);
    SDL_DestroyWindow(window);
    SDL_Quit();
    return 0;
}

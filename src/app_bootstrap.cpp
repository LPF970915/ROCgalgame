#include "app_bootstrap.h"

#ifdef HAVE_SDL2_IMAGE
#include <SDL_image.h>
#endif
#ifdef HAVE_SDL2_TTF
#include <SDL_ttf.h>
#endif

#include <algorithm>
#include <cstdlib>
#include <iostream>
#include <string>

namespace {
int ReadWindowDimension(const char *name, int fallback, int minimum) {
  if (const char *value = std::getenv(name); value && *value) {
    try {
      return std::max(minimum, std::stoi(value));
    } catch (...) {
    }
  }
  return fallback;
}
}  // namespace

AppBootstrapResult BootstrapSdlApp(const LayoutMetrics &layout) {
  AppBootstrapResult result;
  if (SDL_Init(SDL_INIT_VIDEO | SDL_INIT_AUDIO | SDL_INIT_GAMECONTROLLER | SDL_INIT_JOYSTICK | SDL_INIT_EVENTS) != 0) {
    std::cerr << "SDL_Init failed: " << SDL_GetError() << "\n";
    result.exit_code = 1;
    return result;
  }
  result.sdl_initialized = true;

#ifdef HAVE_SDL2_IMAGE
  const int image_flags = IMG_INIT_PNG | IMG_INIT_JPG | IMG_INIT_WEBP;
  result.image_initialized = (IMG_Init(image_flags) & image_flags) != 0;
#endif
#ifdef HAVE_SDL2_TTF
  result.ttf_initialized = TTF_Init() == 0;
#endif

  uint32_t window_flags = SDL_WINDOW_SHOWN;
#if defined(__arm__) || defined(__aarch64__)
  window_flags |= SDL_WINDOW_FULLSCREEN;
#endif
  const int window_w = ReadWindowDimension("ROCGALGAME_WINDOW_W", layout.screen_w, 320);
  const int window_h = ReadWindowDimension("ROCGALGAME_WINDOW_H", layout.screen_h, 240);
  result.window = SDL_CreateWindow("ROCgalgame", SDL_WINDOWPOS_CENTERED, SDL_WINDOWPOS_CENTERED,
                                   window_w, window_h, window_flags);
  if (!result.window) {
    std::cerr << "SDL_CreateWindow failed: " << SDL_GetError() << "\n";
    result.exit_code = 2;
    ShutdownSdlApp(result);
    return result;
  }

  SDL_SetHint(SDL_HINT_RENDER_SCALE_QUALITY, "0");
  result.renderer = SDL_CreateRenderer(result.window, -1, SDL_RENDERER_ACCELERATED | SDL_RENDERER_PRESENTVSYNC);
  if (!result.renderer) result.renderer = SDL_CreateRenderer(result.window, -1, SDL_RENDERER_SOFTWARE);
  if (!result.renderer) {
    std::cerr << "SDL_CreateRenderer failed: " << SDL_GetError() << "\n";
    result.exit_code = 3;
    ShutdownSdlApp(result);
    return result;
  }
  if (window_w != layout.screen_w || window_h != layout.screen_h) {
    SDL_RenderSetLogicalSize(result.renderer, layout.screen_w, layout.screen_h);
  }
  SDL_RendererInfo renderer_info{};
  if (SDL_GetRendererInfo(result.renderer, &renderer_info) == 0) {
    result.renderer_supports_target_textures =
        (renderer_info.flags & SDL_RENDERER_TARGETTEXTURE) != 0;
  }
  result.ok = true;
  return result;
}

void ShutdownSdlApp(AppBootstrapResult &bootstrap) {
  if (bootstrap.renderer) SDL_DestroyRenderer(bootstrap.renderer);
  if (bootstrap.window) SDL_DestroyWindow(bootstrap.window);
  bootstrap.renderer = nullptr;
  bootstrap.window = nullptr;
#ifdef HAVE_SDL2_TTF
  if (bootstrap.ttf_initialized) TTF_Quit();
#endif
#ifdef HAVE_SDL2_IMAGE
  if (bootstrap.image_initialized) IMG_Quit();
#endif
  if (bootstrap.sdl_initialized) SDL_Quit();
  bootstrap.ok = false;
  bootstrap.sdl_initialized = false;
  bootstrap.image_initialized = false;
  bootstrap.ttf_initialized = false;
}

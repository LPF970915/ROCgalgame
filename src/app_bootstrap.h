#pragma once

#include "app_layout.h"

#include <SDL.h>

struct AppBootstrapResult {
  bool ok = false;
  int exit_code = 0;
  SDL_Window *window = nullptr;
  SDL_Renderer *renderer = nullptr;
  bool sdl_initialized = false;
  bool image_initialized = false;
  bool ttf_initialized = false;
  bool renderer_supports_target_textures = false;
};

AppBootstrapResult BootstrapSdlApp(const LayoutMetrics &layout);
void ShutdownSdlApp(AppBootstrapResult &bootstrap);

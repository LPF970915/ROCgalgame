#pragma once

#include "app_layout.h"
#include "screen_profile.h"

#include <SDL.h>

struct AppContext {
  SDL_Window *window = nullptr;
  SDL_Renderer *renderer = nullptr;
  ScreenProfile screen_profile{};
  const LayoutMetrics *layout = nullptr;
  bool renderer_supports_target_textures = false;

  int ScreenWidth() const { return layout ? layout->screen_w : 0; }
  int ScreenHeight() const { return layout ? layout->screen_h : 0; }
};

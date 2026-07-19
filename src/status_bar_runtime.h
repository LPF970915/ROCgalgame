#pragma once

#include "system_status.h"
#include "ui_text_cache.h"

#include <SDL.h>

#include <functional>
#include <string>

struct StatusBarRenderDeps {
  SDL_Renderer *renderer = nullptr;
  const SystemStatusSnapshot *status = nullptr;
  std::string input_profile;
  int screen_w = 0;
  int top_bar_y = 0;
  int top_bar_h = 0;
  SDL_Texture *selected_avatar_texture = nullptr;
  std::function<int(int)> scale_px;
  std::function<void(const SDL_Rect &, SDL_Color, bool)> draw_rect;
  std::function<TextCacheEntry *(const std::string &, SDL_Color)> get_text_texture;
};

void DrawStatusBarRuntime(const StatusBarRenderDeps &deps);

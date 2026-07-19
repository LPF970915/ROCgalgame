#pragma once

#include "ui_text_cache.h"

#include <SDL.h>

#include <cstdint>
#include <functional>

struct VolumeOverlayRenderDeps {
  SDL_Renderer *renderer = nullptr;
  uint32_t now = 0;
  uint32_t display_until = 0;
  int display_percent = 0;
  int top_bar_y = 0;
  int top_bar_h = 0;
  std::function<int(int)> scale_px;
  std::function<TextCacheEntry *(const std::string &, SDL_Color)> get_text_texture;
};

void DrawVolumeOverlayRuntime(const VolumeOverlayRenderDeps &deps);

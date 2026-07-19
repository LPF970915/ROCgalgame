#pragma once

#include "app_layout.h"
#include "cover_cache_runtime.h"
#include "game_library_service.h"
#include "shelf_runtime.h"
#include "ui_assets.h"

#include <SDL.h>

#include <functional>
#include <string>

struct ShelfSceneRenderServices {
  std::function<TextureHandle *(const std::string &)> get_asset;
  std::function<TextureHandle *(const GameEntry &)> get_cover;
  std::function<void(const std::string &, int, int, SDL_Color)> draw_text;
  std::function<void(const std::string &, int &, int &)> measure_text;
  std::function<std::string(const std::string &, int)> ellipsize;
};

struct ShelfSceneRenderContext {
  SDL_Renderer *renderer = nullptr;
  const LayoutMetrics &layout;
  const ShelfRuntimeState &state;
  const GameLibraryService &library;
  ShelfSceneRenderServices services;
};

class ShelfScene {
public:
  bool FocusedTitleNeedsMarquee(const ShelfSceneRenderContext &context) const;
  void Draw(const ShelfSceneRenderContext &context) const;
};

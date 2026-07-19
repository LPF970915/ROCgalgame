#pragma once

#include "ui_assets.h"

#include <cstddef>
#include <deque>
#include <filesystem>
#include <string>

class CoverCacheRuntime {
public:
  explicit CoverCacheRuntime(size_t max_entries = 48) : max_entries_(max_entries) {}

  TextureHandle *Get(SDL_Renderer *renderer, UiAssets &assets,
                     const std::filesystem::path &path);
  void Clear(UiAssets &assets);
  size_t Size() const { return keys_.size(); }

private:
  size_t max_entries_ = 48;
  std::deque<std::string> keys_;
};

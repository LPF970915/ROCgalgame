#include "cover_cache_runtime.h"

#include <algorithm>

namespace {
std::string PathKey(const std::filesystem::path &path) {
#ifdef _WIN32
  return path.u8string();
#else
  return path.native();
#endif
}
}  // namespace

TextureHandle *CoverCacheRuntime::Get(SDL_Renderer *renderer, UiAssets &assets,
                                      const std::filesystem::path &path) {
  if (path.empty()) return nullptr;
  const std::string key = PathKey(path);
  auto found = std::find(keys_.begin(), keys_.end(), key);
  if (found != keys_.end()) {
    keys_.erase(found);
  }
  keys_.push_back(key);
  while (keys_.size() > max_entries_) {
    assets.ReleaseExternal(std::filesystem::u8path(keys_.front()));
    keys_.pop_front();
  }
  return assets.LoadExternal(renderer, path);
}

void CoverCacheRuntime::Clear(UiAssets &assets) {
  keys_.clear();
  assets.ClearExternal();
}

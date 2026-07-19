#pragma once

#include <SDL.h>
#ifdef HAVE_SDL2_TTF
#include <SDL_ttf.h>
#endif

#include <cstddef>
#include <cstdint>
#include <string>
#include <unordered_map>

struct TextCacheEntry {
  SDL_Texture *texture = nullptr;
  int w = 0;
  int h = 0;
  uint32_t last_use = 0;
};

class UiTextCache {
public:
  explicit UiTextCache(size_t max_entries = 512) : max_entries_(max_entries) {}
  ~UiTextCache();

#ifdef HAVE_SDL2_TTF
  TextCacheEntry *Get(SDL_Renderer *renderer, TTF_Font *font,
                      const std::string &text, SDL_Color color);
#endif
  void Clear();
  size_t Size() const { return entries_.size(); }

private:
  void Prune(const std::string &preserve_key);

  size_t max_entries_ = 512;
  std::unordered_map<std::string, TextCacheEntry> entries_;
};

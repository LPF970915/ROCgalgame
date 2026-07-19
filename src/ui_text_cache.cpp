#include "ui_text_cache.h"

#include <cstdint>

namespace {
#ifdef HAVE_SDL2_TTF
std::string MakeKey(TTF_Font *font, const std::string &text, SDL_Color color) {
  return std::to_string(reinterpret_cast<uintptr_t>(font)) + "|" + text + "|" +
         std::to_string(color.r) + "," + std::to_string(color.g) + "," +
         std::to_string(color.b) + "," + std::to_string(color.a);
}
#endif
}  // namespace

UiTextCache::~UiTextCache() { Clear(); }

#ifdef HAVE_SDL2_TTF
TextCacheEntry *UiTextCache::Get(SDL_Renderer *renderer, TTF_Font *font,
                                 const std::string &text, SDL_Color color) {
  if (!renderer || !font || text.empty()) return nullptr;
  const std::string key = MakeKey(font, text, color);
  auto found = entries_.find(key);
  if (found != entries_.end()) {
    found->second.last_use = SDL_GetTicks();
    return &found->second;
  }

  SDL_Surface *surface = TTF_RenderUTF8_Blended(font, text.c_str(), color);
  if (!surface) return nullptr;
  SDL_Texture *texture = SDL_CreateTextureFromSurface(renderer, surface);
  const int width = surface->w;
  const int height = surface->h;
  SDL_FreeSurface(surface);
  if (!texture) return nullptr;

  TextCacheEntry entry;
  entry.texture = texture;
  entry.w = width;
  entry.h = height;
  entry.last_use = SDL_GetTicks();
  entries_.emplace(key, entry);
  Prune(key);
  auto kept = entries_.find(key);
  return kept == entries_.end() ? nullptr : &kept->second;
}
#endif

void UiTextCache::Clear() {
  for (auto &entry : entries_) {
    if (entry.second.texture) SDL_DestroyTexture(entry.second.texture);
  }
  entries_.clear();
}

void UiTextCache::Prune(const std::string &preserve_key) {
  while (entries_.size() > max_entries_) {
    auto oldest = entries_.end();
    for (auto it = entries_.begin(); it != entries_.end(); ++it) {
      if (it->first == preserve_key) continue;
      if (oldest == entries_.end() || it->second.last_use < oldest->second.last_use) oldest = it;
    }
    if (oldest == entries_.end()) break;
    if (oldest->second.texture) SDL_DestroyTexture(oldest->second.texture);
    entries_.erase(oldest);
  }
}

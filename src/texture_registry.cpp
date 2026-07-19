#include "texture_registry.h"

void TextureRegistry::Forget(SDL_Texture *texture) {
  if (texture) sizes_.erase(texture);
}

void TextureRegistry::Remember(SDL_Texture *texture, int width, int height) {
  if (texture) sizes_[texture] = SDL_Point{width, height};
}

bool TextureRegistry::Get(SDL_Texture *texture, int &width, int &height) const {
  const auto found = sizes_.find(texture);
  if (found == sizes_.end()) return false;
  width = found->second.x;
  height = found->second.y;
  return true;
}

void TextureRegistry::Clear() { sizes_.clear(); }

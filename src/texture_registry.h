#pragma once

#include <SDL.h>

#include <unordered_map>

class TextureRegistry {
public:
  void Forget(SDL_Texture *texture);
  void Remember(SDL_Texture *texture, int width, int height);
  bool Get(SDL_Texture *texture, int &width, int &height) const;
  void Clear();

private:
  std::unordered_map<SDL_Texture *, SDL_Point> sizes_;
};

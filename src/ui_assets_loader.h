#pragma once

#include <SDL.h>

#include <filesystem>

struct TextureHandle {
  SDL_Texture *texture = nullptr;
  int w = 0;
  int h = 0;
};

TextureHandle LoadUiTexture(SDL_Renderer *renderer, const std::filesystem::path &path);

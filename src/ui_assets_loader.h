#pragma once

#include <SDL.h>

#include <filesystem>
#include <string>
#include <unordered_map>
#include <vector>

struct TextureHandle {
  SDL_Texture *texture = nullptr;
  int w = 0;
  int h = 0;
};

using UiPackedAssets = std::unordered_map<std::string, std::vector<unsigned char>>;

bool LoadUiAssetPack(const std::filesystem::path &path, UiPackedAssets &assets);
TextureHandle LoadUiTexture(SDL_Renderer *renderer, const std::filesystem::path &path);
TextureHandle LoadUiTexture(SDL_Renderer *renderer, const std::vector<unsigned char> &payload);

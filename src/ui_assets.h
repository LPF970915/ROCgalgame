#pragma once

#include "texture_registry.h"
#include "ui_assets_loader.h"

#include <filesystem>
#include <string>
#include <unordered_map>

class UiAssets {
public:
  ~UiAssets();
  bool Load(SDL_Renderer *renderer, const std::filesystem::path &root, const std::string &profile);
  TextureHandle *Get(const std::string &name);
  size_t LoadedCount() const { return textures_.size(); }
  TextureHandle *LoadExternal(SDL_Renderer *renderer, const std::filesystem::path &path);
  void ReleaseExternal(const std::filesystem::path &path);
  void Clear();
  void ClearExternal();

private:
  TextureHandle LoadTexture(SDL_Renderer *renderer, const std::filesystem::path &path);
  void Destroy(TextureHandle &handle);
  std::unordered_map<std::string, TextureHandle> textures_;
  std::unordered_map<std::string, TextureHandle> external_;
  TextureRegistry registry_;
};

void DrawTexture(SDL_Renderer *renderer, TextureHandle *texture, const SDL_Rect &dst);
void DrawTextureFit(SDL_Renderer *renderer, TextureHandle *texture, const SDL_Rect &dst);

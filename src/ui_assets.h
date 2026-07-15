#pragma once

#include <SDL.h>

#include <filesystem>
#include <string>
#include <unordered_map>

struct TextureHandle {
  SDL_Texture *texture = nullptr;
  int w = 0;
  int h = 0;
};

class UiAssets {
public:
  ~UiAssets();
  bool Load(SDL_Renderer *renderer, const std::filesystem::path &root, const std::string &profile);
  TextureHandle *Get(const std::string &name);
  size_t LoadedCount() const { return textures_.size(); }
  TextureHandle *LoadExternal(SDL_Renderer *renderer, const std::filesystem::path &path);
  void Clear();
  void ClearExternal();

private:
  TextureHandle LoadTexture(SDL_Renderer *renderer, const std::filesystem::path &path);
  std::unordered_map<std::string, TextureHandle> textures_;
  std::unordered_map<std::string, TextureHandle> external_;
};

void DrawTexture(SDL_Renderer *renderer, TextureHandle *texture, const SDL_Rect &dst);
void DrawTextureFit(SDL_Renderer *renderer, TextureHandle *texture, const SDL_Rect &dst);

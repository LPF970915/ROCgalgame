#include "ui_assets_loader.h"

#ifdef HAVE_SDL2_IMAGE
#include <SDL_image.h>
#endif

namespace {
std::string NativePathString(const std::filesystem::path &path) {
#ifdef _WIN32
  return path.u8string();
#else
  return path.native();
#endif
}
}  // namespace

TextureHandle LoadUiTexture(SDL_Renderer *renderer, const std::filesystem::path &path) {
  TextureHandle out;
  std::error_code ec;
  if (!renderer || !std::filesystem::exists(path, ec) || ec) return out;
  std::string filename;
  try {
    filename = NativePathString(path);
  } catch (...) {
    return out;
  }
  if (filename.empty()) return out;
#ifdef HAVE_SDL2_IMAGE
  SDL_Surface *surface = IMG_Load(filename.c_str());
#else
  SDL_Surface *surface = SDL_LoadBMP(filename.c_str());
#endif
  if (!surface) return out;
  out.texture = SDL_CreateTextureFromSurface(renderer, surface);
  out.w = surface->w;
  out.h = surface->h;
  SDL_FreeSurface(surface);
  return out;
}

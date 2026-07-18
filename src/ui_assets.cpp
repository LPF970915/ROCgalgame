#include "ui_assets.h"

#ifdef HAVE_SDL2_IMAGE
#include <SDL_image.h>
#endif

#include <algorithm>
#include <system_error>
#include <vector>

namespace {
std::string NativePathString(const std::filesystem::path &path) {
#ifdef _WIN32
  return path.u8string();
#else
  return path.native();
#endif
}
}  // namespace

UiAssets::~UiAssets() {
  Clear();
}

void UiAssets::Clear() {
  for (auto &[_, tex] : textures_) if (tex.texture) SDL_DestroyTexture(tex.texture);
  for (auto &[_, tex] : external_) if (tex.texture) SDL_DestroyTexture(tex.texture);
  textures_.clear();
  external_.clear();
}

bool UiAssets::Load(SDL_Renderer *renderer, const std::filesystem::path &root, const std::string &profile) {
  const std::vector<std::string> names = {
      "background_main.png",
      "top_status_bar.png",
      "bottom_hint_bar.png",
      "nav_l1_icon.png",
      "nav_r1_icon.png",
      "nav_selected_pill.png",
      "book_under_shadow.png",
      "book_select.png",
      "book_title_shadow.png",
      "book_cover_txt.png",
      "book_cover_pdf.png",
      "Menu_Default.png",
      "Menu_Contact Me.png",
  };
  bool any = false;
  for (const auto &name : names) {
    std::filesystem::path p = root / "ui" / profile / name;
    TextureHandle tex = LoadTexture(renderer, p);
    if (!tex.texture) tex = LoadTexture(renderer, root / "ui" / "common" / name);
    if (tex.texture) {
      textures_[name] = tex;
      any = true;
    }
  }
  return any;
}

TextureHandle *UiAssets::Get(const std::string &name) {
  auto it = textures_.find(name);
  return it == textures_.end() ? nullptr : &it->second;
}

TextureHandle *UiAssets::LoadExternal(SDL_Renderer *renderer, const std::filesystem::path &path) {
  if (path.empty()) return nullptr;
  std::string key;
  try {
    key = NativePathString(path);
  } catch (...) {
    return nullptr;
  }
  if (key.empty()) return nullptr;
  auto found = external_.find(key);
  if (found != external_.end()) return &found->second;
  TextureHandle tex = LoadTexture(renderer, path);
  if (!tex.texture) return nullptr;
  auto inserted = external_.emplace(key, tex);
  return &inserted.first->second;
}

void UiAssets::ClearExternal() {
  for (auto &[_, tex] : external_) if (tex.texture) SDL_DestroyTexture(tex.texture);
  external_.clear();
}

TextureHandle UiAssets::LoadTexture(SDL_Renderer *renderer, const std::filesystem::path &path) {
  TextureHandle out;
  std::error_code ec;
  if (!std::filesystem::exists(path, ec)) return out;
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

void DrawTexture(SDL_Renderer *renderer, TextureHandle *texture, const SDL_Rect &dst) {
  if (!texture || !texture->texture) return;
  SDL_RenderCopy(renderer, texture->texture, nullptr, &dst);
}

void DrawTextureFit(SDL_Renderer *renderer, TextureHandle *texture, const SDL_Rect &dst) {
  if (!texture || !texture->texture || texture->w <= 0 || texture->h <= 0) return;
  const float src_aspect = static_cast<float>(texture->w) / static_cast<float>(texture->h);
  const float dst_aspect = static_cast<float>(dst.w) / static_cast<float>(dst.h);
  SDL_Rect fit = dst;
  if (src_aspect > dst_aspect) {
    fit.h = std::max(1, static_cast<int>(dst.w / src_aspect));
    fit.y = dst.y + (dst.h - fit.h) / 2;
  } else {
    fit.w = std::max(1, static_cast<int>(dst.h * src_aspect));
    fit.x = dst.x + (dst.w - fit.w) / 2;
  }
  SDL_RenderCopy(renderer, texture->texture, nullptr, &fit);
}

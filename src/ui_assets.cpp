#include "ui_assets.h"

#include <algorithm>
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
  for (auto &[_, tex] : textures_) Destroy(tex);
  for (auto &[_, tex] : external_) Destroy(tex);
  textures_.clear();
  external_.clear();
  registry_.Clear();
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

void UiAssets::ReleaseExternal(const std::filesystem::path &path) {
  std::string key;
  try {
    key = NativePathString(path);
  } catch (...) {
    return;
  }
  auto found = external_.find(key);
  if (found == external_.end()) return;
  Destroy(found->second);
  external_.erase(found);
}

void UiAssets::ClearExternal() {
  for (auto &[_, tex] : external_) Destroy(tex);
  external_.clear();
}

TextureHandle UiAssets::LoadTexture(SDL_Renderer *renderer, const std::filesystem::path &path) {
  TextureHandle out = LoadUiTexture(renderer, path);
  registry_.Remember(out.texture, out.w, out.h);
  return out;
}

void UiAssets::Destroy(TextureHandle &handle) {
  if (!handle.texture) return;
  registry_.Forget(handle.texture);
  SDL_DestroyTexture(handle.texture);
  handle = {};
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

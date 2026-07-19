#include "ui_assets_loader.h"

#ifdef HAVE_SDL2_IMAGE
#include <SDL_image.h>
#endif

#include <cstdint>
#include <fstream>
#include <limits>

namespace {
std::string NativePathString(const std::filesystem::path &path) {
#ifdef _WIN32
  return path.u8string();
#else
  return path.native();
#endif
}

void XorUiPayload(std::vector<unsigned char> &data, const std::string &name) {
  static const std::string key = "ROCgalgame::native_gkd350h::ui_pack";
  if (name.empty()) return;
  for (size_t i = 0; i < data.size(); ++i) {
    const auto k = static_cast<unsigned char>(key[i % key.size()]);
    const auto n = static_cast<unsigned char>(name[i % name.size()]);
    data[i] = static_cast<unsigned char>(
        data[i] ^ k ^ n ^ static_cast<unsigned char>((i * 131u) & 0xffu));
  }
}

TextureHandle TextureFromSurface(SDL_Renderer *renderer, SDL_Surface *surface) {
  TextureHandle out;
  if (!renderer || !surface) return out;
  out.texture = SDL_CreateTextureFromSurface(renderer, surface);
  out.w = surface->w;
  out.h = surface->h;
  return out;
}
}  // namespace

bool LoadUiAssetPack(const std::filesystem::path &path, UiPackedAssets &assets) {
  assets.clear();
  std::ifstream in(path, std::ios::binary);
  if (!in) return false;

  char magic[8] = {};
  in.read(magic, sizeof(magic));
  if (!in || std::string(magic, sizeof(magic)) != "RCUIPK01") return false;

  uint32_t count = 0;
  in.read(reinterpret_cast<char *>(&count), sizeof(count));
  if (!in || count > 10000u) return false;

  UiPackedAssets loaded;
  loaded.reserve(count);
  for (uint32_t i = 0; i < count; ++i) {
    uint16_t name_len = 0;
    uint32_t original_size = 0;
    uint32_t encrypted_size = 0;
    in.read(reinterpret_cast<char *>(&name_len), sizeof(name_len));
    if (!in || name_len == 0 || name_len > 4096u) return false;

    std::string name(name_len, '\0');
    in.read(name.data(), static_cast<std::streamsize>(name.size()));
    in.read(reinterpret_cast<char *>(&original_size), sizeof(original_size));
    in.read(reinterpret_cast<char *>(&encrypted_size), sizeof(encrypted_size));
    if (!in || original_size != encrypted_size || encrypted_size > 256u * 1024u * 1024u) {
      return false;
    }

    std::vector<unsigned char> payload(encrypted_size);
    if (!payload.empty()) {
      in.read(reinterpret_cast<char *>(payload.data()), static_cast<std::streamsize>(payload.size()));
    }
    if (!in) return false;
    XorUiPayload(payload, name);
    if (!loaded.emplace(std::move(name), std::move(payload)).second) return false;
  }

  assets = std::move(loaded);
  return !assets.empty();
}

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
  out = TextureFromSurface(renderer, surface);
  SDL_FreeSurface(surface);
  return out;
}

TextureHandle LoadUiTexture(SDL_Renderer *renderer, const std::vector<unsigned char> &payload) {
  TextureHandle out;
  if (!renderer || payload.empty() ||
      payload.size() > static_cast<size_t>(std::numeric_limits<int>::max())) {
    return out;
  }
  SDL_RWops *rw = SDL_RWFromConstMem(payload.data(), static_cast<int>(payload.size()));
  if (!rw) return out;
#ifdef HAVE_SDL2_IMAGE
  SDL_Surface *surface = IMG_Load_RW(rw, 1);
#else
  SDL_Surface *surface = SDL_LoadBMP_RW(rw, 1);
#endif
  if (!surface) return out;
  out = TextureFromSurface(renderer, surface);
  SDL_FreeSurface(surface);
  return out;
}

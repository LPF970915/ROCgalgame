#include "contributor_avatar_panel.h"

#include <algorithm>

namespace {
std::string AvatarDisplayName(const std::string &filename) {
  size_t begin = filename.find('_');
  if (begin == std::string::npos) return filename;
  begin = filename.find('_', begin + 1);
  if (begin == std::string::npos) return filename;
  ++begin;
  size_t end = filename.find(u8"_贡献值", begin);
  if (end == std::string::npos) end = filename.find_last_of('.');
  return filename.substr(begin, end == std::string::npos ? std::string::npos : end - begin);
}

void DrawTextureCrop(SDL_Renderer *renderer, TextureHandle *texture, const SDL_Rect &dst) {
  if (!renderer || !texture || !texture->texture || texture->w <= 0 || texture->h <= 0) return;
  const int side = std::min(texture->w, texture->h);
  const SDL_Rect src{(texture->w - side) / 2, (texture->h - side) / 2, side, side};
  SDL_RenderCopy(renderer, texture->texture, &src, &dst);
}
}  // namespace

void DrawContributorAvatarPanel(
    SDL_Renderer *renderer, UiAssets &assets, const SDL_Rect &preview, int bottom_bar_y,
    const std::vector<ContributorAvatarEntry> &entries,
    const ContributorAvatarState &state, const SettingsRuntimeState &menu_state,
    const MenuPanelDrawServices &services) {
  if (!renderer || entries.empty()) return;
  const int safe_left = preview.x + 48;
  const int safe_right = preview.x + preview.w - 48;
  const int safe_top = preview.y + 100;
  const int safe_bottom = bottom_bar_y - 40;
  const int gap_x = 28;
  const int tile_w = (safe_right - safe_left - gap_x * 2) / 3;
  const int row_pitch = 410;
  const int image_size = 280;
  const SDL_Rect viewport{safe_left, safe_top, safe_right - safe_left, safe_bottom - safe_top};
  SDL_Rect old_clip{};
  const SDL_bool had_clip = SDL_RenderIsClipEnabled(renderer);
  if (had_clip == SDL_TRUE) SDL_RenderGetClipRect(renderer, &old_clip);
  SDL_RenderSetClipRect(renderer, &viewport);
  for (int i = 0; i < static_cast<int>(entries.size()); ++i) {
    const int row = i / 3;
    const int col = i % 3;
    const int tile_x = safe_left + col * (tile_w + gap_x);
    const int tile_y = safe_top + 16 + (row - state.scroll_row) * row_pitch;
    if (tile_y + image_size < safe_top || tile_y > safe_bottom) continue;
    const bool focused = menu_state.panel_active && i == state.focus_index;
    const int draw_size = focused ? 302 : image_size;
    const int image_x = tile_x + (tile_w - draw_size) / 2;
    TextureHandle *avatar = assets.LoadExternal(renderer, entries[static_cast<size_t>(i)].path);
    DrawTextureCrop(renderer, avatar, SDL_Rect{image_x, tile_y, draw_size, draw_size});
    int frame_no = -1;
    if (entries[static_cast<size_t>(i)].contribution_is_max) frame_no = 0;
    else if (i < 3) frame_no = i + 1;
    if (frame_no >= 0) {
      TextureHandle *frame = assets.LoadExternal(
          renderer, entries[static_cast<size_t>(i)].path.parent_path() /
                        ("AvatarFrame_NO." + std::to_string(frame_no) + ".png"));
      DrawTexture(renderer, frame, SDL_Rect{image_x, tile_y, draw_size, draw_size});
    }
    if (focused) {
      services.draw_rect(SDL_Rect{image_x - 6, tile_y - 6, draw_size + 12, draw_size + 12},
                         SDL_Color{120, 205, 255, 255}, false, 4);
    }
    std::string label = AvatarDisplayName(entries[static_cast<size_t>(i)].filename);
    if (services.ellipsize) label = services.ellipsize(label, MenuPanelTextStyle::Small, tile_w);
    if (services.draw_text_centered) {
      services.draw_text_centered(label,
                                  SDL_Rect{tile_x, tile_y + draw_size + 18, tile_w, 58},
                                  SDL_Color{235, 241, 248, 255}, MenuPanelTextStyle::Small);
    }
  }
  SDL_RenderSetClipRect(renderer, had_clip == SDL_TRUE ? &old_clip : nullptr);
}

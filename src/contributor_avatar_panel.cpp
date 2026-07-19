#include "contributor_avatar_panel.h"

#include "app_language.h"

#include <algorithm>
#include <cmath>

namespace {
constexpr float kAvatarNameMarqueeGapPx = 4.0f;

std::string AvatarRawLabel(const std::string &filename) {
  size_t begin = filename.find('_');
  if (begin == std::string::npos) return filename;
  begin = filename.find('_', begin + 1);
  if (begin == std::string::npos) return filename;
  ++begin;
  const size_t end = filename.find_last_of('.');
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
    SDL_Renderer *renderer, UiAssets &assets, const SDL_Rect &preview,
    const std::vector<ContributorAvatarEntry> &entries,
    const ContributorAvatarState &state, const SettingsRuntimeState &menu_state,
    int language_index, const MenuPanelDrawServices &services) {
  if (!renderer || entries.empty()) return;
  constexpr float ui_scale = 2.0f;
  const int safe_left = preview.x + 48;
  const int safe_right = preview.x + preview.w - 48;
  const int safe_top = preview.y + 100;
  const int safe_bottom = preview.y + preview.h - 96;
  const int safe_w = std::max(0, safe_right - safe_left);
  const int safe_h = std::max(0, safe_bottom - safe_top);
  if (safe_w <= 0 || safe_h <= 0) return;
  const int col_gap = 28;
  const int row_gap = 40;
  const int name_gap = 24;
  const int name_h = 68;
  const int tile_w = std::max(120, (safe_w - col_gap * 2) / 3);
  const int row_pitch = std::clamp(
      static_cast<int>(std::floor((safe_h - row_gap * 1.5f) / 2.5f)), 300, 424);
  const int image_size = std::max(144, std::min(tile_w, row_pitch - name_gap - name_h - row_gap));
  const int x_base = safe_left + std::max(0, (safe_w - (tile_w * 3 + col_gap * 2)) / 2);
  const int y_base = safe_top + std::max(24, safe_h / 24);
  const int scroll_y = state.scroll_row * row_pitch;
  const SDL_Rect viewport{safe_left, safe_top, safe_right - safe_left, safe_bottom - safe_top};
  SDL_Rect old_clip{};
  const SDL_bool had_clip = SDL_RenderIsClipEnabled(renderer);
  if (had_clip == SDL_TRUE) SDL_RenderGetClipRect(renderer, &old_clip);
  SDL_RenderSetClipRect(renderer, &viewport);
  for (int i = 0; i < static_cast<int>(entries.size()); ++i) {
    const int row = i / 3;
    const int col = i % 3;
    const int tile_x = x_base + col * (tile_w + col_gap);
    const int tile_y = y_base + row * row_pitch - scroll_y;
    const bool focused = menu_state.panel_active && i == state.focus_index;
    const int draw_size = static_cast<int>(std::lround(image_size * (focused ? 1.08f : 1.0f)));
    const int image_x = tile_x + (tile_w - draw_size) / 2;
    const int label_w = image_size;
    const int label_x = tile_x + (tile_w - label_w) / 2;
    const int label_y = tile_y + draw_size + name_gap;
    if (label_y + name_h < safe_top || tile_y > safe_bottom) continue;
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
                         SDL_Color{120, 205, 255, 255}, false, 2);
    }
    const std::string label = LocalizeContributionLabel(
        language_index, AvatarRawLabel(entries[static_cast<size_t>(i)].filename));
    if (services.get_text_texture && !label.empty()) {
      TextCacheEntry *label_texture = services.get_text_texture(
          label, SDL_Color{235, 241, 248, 255}, MenuPanelTextStyle::Menu);
      if (label_texture && label_texture->texture) {
        const SDL_Rect label_clip{label_x, label_y, label_w, name_h};
        SDL_RenderSetClipRect(renderer, &label_clip);
        const int draw_y = label_y + std::max(0, (name_h - label_texture->h) / 2);
        if (label_texture->w <= label_w) {
          const int centered_x = label_x + std::max(0, (label_w - label_texture->w) / 2);
          const SDL_Rect dst{centered_x, draw_y, label_texture->w, label_texture->h};
          SDL_RenderCopy(renderer, label_texture->texture, nullptr, &dst);
        } else if (focused) {
          const float span = static_cast<float>(label_texture->w) +
                             kAvatarNameMarqueeGapPx * ui_scale;
          const float offset = span > 0.0f ? std::fmod(state.marquee_offset, span) : 0.0f;
          const SDL_Rect first{label_x - static_cast<int>(std::lround(offset)), draw_y,
                               label_texture->w, label_texture->h};
          const SDL_Rect second{first.x + static_cast<int>(std::lround(span)), draw_y,
                                label_texture->w, label_texture->h};
          SDL_RenderCopy(renderer, label_texture->texture, nullptr, &first);
          SDL_RenderCopy(renderer, label_texture->texture, nullptr, &second);
        } else {
          const SDL_Rect dst{label_x, draw_y, label_texture->w, label_texture->h};
          SDL_RenderCopy(renderer, label_texture->texture, nullptr, &dst);
        }
        SDL_RenderSetClipRect(renderer, &viewport);
      }
    }
  }
  SDL_RenderSetClipRect(renderer, had_clip == SDL_TRUE ? &old_clip : nullptr);
}

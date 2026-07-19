#include "status_bar_runtime.h"

#include <algorithm>
#include <string>

void DrawStatusBarRuntime(const StatusBarRenderDeps &deps) {
  if (!deps.renderer || !deps.status) return;
  const SystemStatusSnapshot &status = *deps.status;
  const SDL_Color text_color{238, 242, 250, 255};
  const SDL_Color fill_color = status.charging ? SDL_Color{104, 214, 141, 255} : text_color;
  const bool gkd = deps.input_profile == "gkd350h-ultra" || deps.input_profile == "gkd350h";
  auto scale_px = [&](int value) { return deps.scale_px ? deps.scale_px(value) : value; };
  const int center_y = deps.top_bar_y + deps.top_bar_h / 2;
  const int extra_x = std::max(0, deps.screen_w - scale_px(720));
  const int clock_right = gkd ? 1468 : scale_px(664) + extra_x;

#ifdef HAVE_SDL2_TTF
  if (!status.clock_text.empty() && deps.get_text_texture) {
    TextCacheEntry *clock = deps.get_text_texture(status.clock_text, text_color);
    if (clock && clock->texture) {
      const SDL_Rect dst{clock_right - clock->w, center_y - clock->h / 2,
                         clock->w, clock->h};
      SDL_RenderCopy(deps.renderer, clock->texture, nullptr, &dst);
    }
  }
#endif

  const int avatar_size = gkd ? 64 : scale_px(28);
  const SDL_Rect avatar{gkd ? 1512 : deps.screen_w - scale_px(12) - avatar_size,
                        gkd ? 8 : scale_px(4), avatar_size, avatar_size};
  if (deps.draw_rect) {
    deps.draw_rect(avatar, SDL_Color{26, 32, 42, 220}, true);
    deps.draw_rect(avatar, SDL_Color{152, 185, 210, 235}, false);
  }
  if (deps.selected_avatar_texture) {
    SDL_RenderCopy(deps.renderer, deps.selected_avatar_texture, nullptr, &avatar);
  }

  if (!status.battery_available) return;
  const int body_w = gkd ? 47 : scale_px(24);
  const int body_h = gkd ? 29 : scale_px(12);
  const int icon_x = gkd ? 1249 : scale_px(552) + extra_x;
  const int icon_y = gkd ? 26 : center_y - body_h / 2 + scale_px(3);
  if (deps.draw_rect) {
    const int outline_width = gkd ? 3 : 1;
    for (int inset = 0; inset < outline_width; ++inset) {
      deps.draw_rect(SDL_Rect{icon_x + inset, icon_y + inset,
                             body_w - inset * 2, body_h - inset * 2}, text_color, false);
    }
    const int cap_w = gkd ? 4 : scale_px(4);
    const int cap_h = gkd ? 10 : scale_px(8);
    deps.draw_rect(SDL_Rect{icon_x + body_w, center_y - cap_h / 2, cap_w, cap_h}, text_color, true);
    const int pad = gkd ? 5 : scale_px(2);
    const int inner_w = body_w - pad * 2;
    const int fill_w = std::clamp(inner_w * status.battery_percent / 100, 0, inner_w);
    if (fill_w > 0) {
      deps.draw_rect(SDL_Rect{icon_x + pad, icon_y + pad, fill_w, body_h - pad * 2}, fill_color, true);
    }
  }

#ifdef HAVE_SDL2_TTF
  if (deps.get_text_texture) {
    TextCacheEntry *battery = deps.get_text_texture(std::to_string(status.battery_percent) + "%", text_color);
    if (battery && battery->texture) {
      const int text_x = gkd ? 1308 : scale_px(587) + extra_x;
      const SDL_Rect dst{text_x, center_y - battery->h / 2, battery->w, battery->h};
      SDL_RenderCopy(deps.renderer, battery->texture, nullptr, &dst);
    }
  }
#endif
}

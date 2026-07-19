#pragma once

#include "ui_text_cache.h"

#include <SDL.h>

#include <functional>
#include <string>

enum class MenuPanelTextStyle {
  Title,
  Menu,
  Body,
  Small,
};

struct MenuPanelDrawServices {
  std::function<void(const SDL_Rect &, SDL_Color, bool, int)> draw_rect;
  std::function<void(const std::string &, int, int, SDL_Color, MenuPanelTextStyle)> draw_text;
  std::function<void(const std::string &, const SDL_Rect &, SDL_Color, MenuPanelTextStyle)> draw_text_centered;
  std::function<void(const std::string &, int, int, SDL_Color, MenuPanelTextStyle)> draw_text_right;
  std::function<void(const std::string &, MenuPanelTextStyle, int &, int &)> measure_text;
  std::function<std::string(const std::string &, MenuPanelTextStyle, int)> ellipsize;
  std::function<TextCacheEntry *(const std::string &, SDL_Color, MenuPanelTextStyle)>
      get_text_texture;
};

inline void DrawMenuPanelHeader(const SDL_Rect &preview, int first_row_y,
                                const std::string &title,
                                const MenuPanelDrawServices &services) {
  const int divider_y = first_row_y - 36;
  int title_w = 0;
  int title_h = 0;
  if (services.measure_text) {
    services.measure_text(title, MenuPanelTextStyle::Title, title_w, title_h);
  }
  if (services.draw_text) {
    services.draw_text(title, preview.x + 64, divider_y - title_h - 24,
                       SDL_Color{240, 246, 255, 255}, MenuPanelTextStyle::Title);
  }
  if (services.draw_rect) {
    services.draw_rect(SDL_Rect{preview.x + 36, divider_y, preview.w - 72, 2},
                       SDL_Color{66, 95, 124, 255}, true, 1);
  }
}

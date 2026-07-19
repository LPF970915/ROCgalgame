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
};

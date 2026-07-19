#pragma once

#include "app_layout.h"
#include "menu_panel.h"

#include <SDL.h>

void DrawContactPanel(const SDL_Rect &preview, const LayoutMetrics &layout, int language_index,
                      const MenuPanelDrawServices &services);

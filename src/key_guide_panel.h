#pragma once

#include "menu_panel.h"

#include <SDL.h>

void DrawKeyGuidePanel(const SDL_Rect &preview, int first_row_y,
                       const MenuPanelDrawServices &services);

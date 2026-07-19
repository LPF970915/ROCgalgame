#pragma once

#include "menu_panel.h"

#include <SDL.h>

void DrawExitPanel(const SDL_Rect &preview, int first_row_y, int language_index,
                   const MenuPanelDrawServices &services);

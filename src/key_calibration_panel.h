#pragma once

#include "menu_panel.h"

#include <SDL.h>

#include <string>

struct KeyCalibrationPanelModel {
  bool capturing = false;
  bool complete = false;
  bool failed = false;
  bool panel_active = false;
  int current = 0;
  int total = 0;
  std::string current_button;
  std::string status;
};

void DrawKeyCalibrationPanel(const SDL_Rect &preview,
                             const KeyCalibrationPanelModel &model,
                             const MenuPanelDrawServices &services);

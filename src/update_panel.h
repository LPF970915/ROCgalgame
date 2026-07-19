#pragma once

#include "menu_panel.h"
#include "version_update_runtime.h"

#include <SDL.h>

#include <string>

struct UpdatePanelModel {
  int language_index = 0;
  VersionUpdateStatus status = VersionUpdateStatus::Idle;
  bool panel_active = false;
  std::string current_version;
  std::string latest_version;
  int download_progress_pct = 0;
};

void DrawUpdatePanel(const SDL_Rect &preview, int first_row_y,
                     const UpdatePanelModel &model,
                     const MenuPanelDrawServices &services);

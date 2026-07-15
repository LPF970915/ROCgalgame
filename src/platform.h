#pragma once

#include "config.h"

struct PlatformLayout {
  int screen_w = 1600;
  int screen_h = 1440;
  int top_bar_y = 0;
  int top_bar_h = 80;
  int bottom_bar_y = 1360;
  int bottom_bar_h = 80;
  int nav_l1_x = 48;
  int nav_l1_y = 104;
  int nav_r1_x = 1480;
  int nav_r1_y = 104;
  int nav_start_x = 196;
  int nav_slot_w = 300;
  int nav_y = 96;
  int nav_pill_h = 80;
  int grid_x = 74;
  int grid_y = 222;
  int cover_w = 314;
  int cover_h = 471;
  int card_frame_w = 394;
  int card_frame_h = 551;
  int step_x = 386;
  int step_y = 541;
  int title_overlay_h = 72;
  int title_text_pad_x = 4;
  int title_text_pad_bottom = 8;
  int title_marquee_gap_px = 48;
  int settings_sidebar_w = 480;
  int settings_y_offset = 0;
  int settings_content_offset_y = 84;
  int cols = 4;
  int rows = 3;
  float focus_scale = 1.045f;
  float ui_scale = 2.0f;
};

PlatformLayout ResolveLayout(const AppConfig &config);

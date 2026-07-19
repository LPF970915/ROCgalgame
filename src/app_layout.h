#pragma once

#include "config.h"
#include "screen_profile.h"

struct LayoutMetrics {
  int screen_w = 720;
  int screen_h = 480;
  int safe_margin_x = 20;
  int top_bar_y = 0;
  int top_bar_h = 30;
  int nav_bar_y = 30;
  int nav_bar_h = 50;
  int main_grid_y = 80;
  int main_grid_h = 350;
  int bottom_bar_y = 430;
  int bottom_bar_h = 50;
  int cover_w = 140;
  int cover_h = 210;
  int card_frame_w = 180;
  int card_frame_h = 250;
  int grid_gap_x = 33;
  int grid_gap_y = 38;
  int grid_start_x = 33;
  int grid_start_y = 100;
  int title_overlay_h = 36;
  int title_text_pad_x = 2;
  int title_text_pad_bottom = 4;
  int title_marquee_gap_px = 24;
  int settings_sidebar_w = 240;
  int settings_y_offset = 0;
  int settings_content_offset_y = 35;
  int txt_margin_x = 32;
  int txt_margin_y = 20;
  int nav_l1_x = 21;
  int nav_l1_y = 46;
  int nav_r1_x = 667;
  int nav_r1_y = 46;
  int nav_start_x = 90;
  int nav_slot_w = 135;
  int nav_y = 42;
  int nav_pill_h = 32;
  int reader_progress_panel_margin_x = 18;
  int reader_progress_panel_margin_bottom = 12;
  int reader_progress_bar_margin_x = 34;
  int reader_progress_percent_margin_x = 34;
  int grid_cols = 4;
  int visible_rows = 2;
  float ui_scale = 1.0f;
};

const LayoutMetrics &SelectLayoutProfile(int screen_w, int screen_h);
LayoutMetrics ResolveLayout(const AppConfig &config, const ScreenProfile &profile);
int FocusedCoverWidth(const LayoutMetrics &layout);
int FocusedCoverHeight(const LayoutMetrics &layout);
int GridStepX(const LayoutMetrics &layout);
int GridStepY(const LayoutMetrics &layout);

#pragma once

#include "animation.h"
#include "config.h"
#include "input_manager.h"
#include "settings_panel_router.h"
#include "ui_assets.h"
#include "ui_text_cache.h"

#include <SDL.h>

#include <functional>
#include <string>
#include <vector>

enum class SettingId {
  KeyGuide,
  KeyCalibration,
  SystemControls,
  GameSettings,
  ContributorAvatars,
  ContactMe,
  VersionUpdate,
  ExitApp,
};

struct SettingsRuntimeState {
  animation::TweenFloat anim{0.0f};
  bool closing = false;
  float toggle_guard = 0.0f;
  bool close_armed = true;
  int selected = 0;
  std::vector<SettingId> items;
  bool panel_active = false;
  int panel_focus = 0;
  int panel_button = 0;
};

struct SettingsRuntimeInputActions {
  bool menu_toggle_request = false;
  bool animations_enabled = true;
  std::function<void()> on_close;
  std::function<void()> on_exit_app;
  std::function<void(SettingId, SettingsRuntimeState &)> on_open_panel;
  SettingsPanelInputHandlers panel_handlers;
};

struct SettingsRuntimeInputDeps {
  const InputManager &input;
  float dt = 0.0f;
  SettingsRuntimeState &state;
  SettingsRuntimeInputActions actions;
};

struct SettingsRuntimeLayout {
  int screen_w = 0;
  int screen_h = 0;
  int top_bar_y = 0;
  int top_bar_h = 0;
  int bottom_bar_y = 0;
  int bottom_bar_h = 0;
  int settings_sidebar_w = 0;
  int settings_y_offset = 0;
  int settings_content_offset_y = 0;
  float ui_scale = 1.0f;
};

struct SettingsRuntimeRenderServices {
  std::function<void(int, int, int, int, SDL_Color, bool)> draw_rect;
  std::function<TextureHandle *(const std::string &)> get_texture;
  std::function<TextCacheEntry *(const std::string &, SDL_Color)> get_text_texture;
  std::function<TextCacheEntry *(const std::string &, SDL_Color)> get_title_text_texture;
  std::function<void(TextureHandle *, const SDL_Rect &)> draw_texture_fit;
  std::function<void()> draw_chrome;
};

struct SettingsRuntimeRenderDeps {
  SDL_Renderer *renderer = nullptr;
  const AppConfig &cfg;
  InputProfile input_profile = InputProfile::DesktopDefault;
  SettingsRuntimeState &state;
  SettingsRuntimeLayout layout;
  SettingsRuntimeRenderServices services;
  SettingsPanelDrawHandlers panel_draw_handlers;
};

const char *SettingPreviewAsset(SettingId id);
std::string SettingLabel(SettingId id, int language_index);
void HandleSettingsInput(SettingsRuntimeInputDeps &deps);
void DrawSettingsRuntime(SettingsRuntimeRenderDeps &deps);

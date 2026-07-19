#pragma once

#include <SDL.h>

#include <functional>

enum class SettingId;
struct SettingsRuntimeState;
struct SettingsRuntimeRenderDeps;

struct SettingsPanelInputHandlers {
  std::function<bool(SettingsRuntimeState &)> system_controls;
  std::function<bool(SettingsRuntimeState &)> game_settings;
  std::function<bool(SettingsRuntimeState &)> key_calibration;
  std::function<bool(SettingsRuntimeState &)> contributor_avatars;
  std::function<bool(SettingsRuntimeState &)> version_update;
};

struct SettingsPanelDrawHandlers {
  std::function<void(const SDL_Rect &, int, int, int)> system_controls;
  std::function<void(const SDL_Rect &, int, int, int)> game_settings;
  std::function<void(const SDL_Rect &, int)> key_guide;
  std::function<void(const SDL_Rect &, int)> key_calibration;
  std::function<void(const SDL_Rect &, int)> contributor_avatars;
  std::function<void(const SDL_Rect &, int)> contact;
  std::function<void(const SDL_Rect &, int)> version_update;
  std::function<void(const SDL_Rect &, int)> exit_app;
};

bool HandleSelectedSettingsPanelInput(SettingId id, SettingsRuntimeState &state,
                                      const SettingsPanelInputHandlers &handlers);
void DrawSelectedSettingsPanel(SettingId id, SettingsRuntimeRenderDeps &deps,
                               const SDL_Rect &preview_rect, int first_menu_item_y,
                               int sidebar_item_pitch, int sidebar_item_h);

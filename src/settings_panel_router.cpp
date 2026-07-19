#include "settings_panel_router.h"

#include "settings_runtime.h"

bool HandleSelectedSettingsPanelInput(SettingId id, SettingsRuntimeState &state,
                                      const SettingsPanelInputHandlers &handlers) {
  switch (id) {
    case SettingId::SystemControls:
      return handlers.system_controls && handlers.system_controls(state);
    case SettingId::GameSettings:
      return handlers.game_settings && handlers.game_settings(state);
    case SettingId::KeyCalibration:
      return handlers.key_calibration && handlers.key_calibration(state);
    case SettingId::ContributorAvatars:
      return handlers.contributor_avatars && handlers.contributor_avatars(state);
    case SettingId::VersionUpdate:
      return handlers.version_update && handlers.version_update(state);
    default:
      return false;
  }
}

void DrawSelectedSettingsPanel(SettingId id, SettingsRuntimeRenderDeps &deps,
                               const SDL_Rect &preview_rect, int first_menu_item_y,
                               int sidebar_item_pitch, int sidebar_item_h) {
  const SettingsPanelDrawHandlers &draw = deps.panel_draw_handlers;
  switch (id) {
    case SettingId::SystemControls:
      if (draw.system_controls) draw.system_controls(preview_rect, first_menu_item_y,
                                                     sidebar_item_pitch, sidebar_item_h);
      break;
    case SettingId::GameSettings:
      if (draw.game_settings) draw.game_settings(preview_rect, first_menu_item_y,
                                                 sidebar_item_pitch, sidebar_item_h);
      break;
    case SettingId::KeyGuide:
      if (draw.key_guide) draw.key_guide(preview_rect, first_menu_item_y);
      break;
    case SettingId::KeyCalibration:
      if (draw.key_calibration) draw.key_calibration(preview_rect, first_menu_item_y);
      break;
    case SettingId::ContributorAvatars:
      if (draw.contributor_avatars) draw.contributor_avatars(preview_rect, first_menu_item_y);
      break;
    case SettingId::ContactMe:
      if (draw.contact) draw.contact(preview_rect, first_menu_item_y);
      break;
    case SettingId::VersionUpdate:
      if (draw.version_update) draw.version_update(preview_rect, first_menu_item_y);
      break;
    case SettingId::ExitApp:
      if (draw.exit_app) draw.exit_app(preview_rect, first_menu_item_y);
      break;
  }
}

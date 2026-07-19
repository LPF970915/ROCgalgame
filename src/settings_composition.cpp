#include "settings_composition.h"

#include "app_language.h"
#include "contact_panel.h"
#include "contributor_avatar_panel.h"
#include "exit_panel.h"
#include "game_settings_panel.h"
#include "key_calibration_panel.h"
#include "key_guide_panel.h"
#include "system_controls_panel.h"
#include "update_panel.h"

#include <algorithm>

SettingsPanelInputHandlers MakeSettingsPanelInputHandlers(
    SettingsPanelInputComposition composition) {
  SettingsPanelInputHandlers handlers;
  handlers.system_controls = [input = &composition.input,
                              adjust = std::move(composition.adjust_system_setting)](
                                 SettingsRuntimeState &state) {
    return HandleSystemControlsPanelInput(*input, state, adjust);
  };
  handlers.game_settings = [input = &composition.input,
                            game_settings = &composition.game_settings,
                            callbacks = std::move(composition.game_settings_callbacks)](
                               SettingsRuntimeState &state) {
    return HandleGameSettingsPanelInput(*input, state, *game_settings, callbacks);
  };
  handlers.key_calibration = [input = &composition.input,
                              calibration = &composition.calibration,
                              save = std::move(composition.save_key_mapping),
                              play_select = std::move(composition.play_select_sfx)](
                                 SettingsRuntimeState &state) {
    return HandleKeyCalibrationInput(
        *input, *calibration, SDL_GetTicks(),
        KeyCalibrationCallbacks{
            save,
            play_select,
            [&state]() { state.panel_active = false; },
        });
  };
  handlers.contributor_avatars = [input = &composition.input,
                                  contributor = &composition.contributor_avatar,
                                  count = composition.contributor_avatar_count,
                                  confirm = std::move(composition.confirm_contributor_avatar)](
                                     SettingsRuntimeState &state) {
    return HandleContributorAvatarInput(*input, state, *contributor, count, confirm);
  };
  handlers.version_update = [input = &composition.input,
                             update = &composition.version_update,
                             configured = composition.update_manifest_configured](
                                SettingsRuntimeState &state) {
    return HandleVersionUpdateInput(
        *input, *update,
        VersionUpdateCallbacks{
            [&state]() { state.panel_active = false; },
            [configured](VersionUpdateState &version_state) {
              version_state.status = configured ? VersionUpdateStatus::Checking
                                                : VersionUpdateStatus::Unconfigured;
            },
        });
  };
  return handlers;
}

SettingsPanelDrawHandlers MakeSettingsPanelDrawHandlers(
    SettingsPanelDrawComposition composition) {
  SettingsPanelDrawHandlers panels;
  const int language = SystemLanguageIndexFromConfigValue(composition.config.system_language);
  panels.system_controls = [menu = &composition.menu_state,
                            config = &composition.config,
                            levels = composition.system_levels,
                            language,
                            services = composition.services](const SDL_Rect &preview,
                                                              int first_y, int pitch,
                                                              int height) {
    DrawSystemControlsPanel(
        preview, first_y, pitch, height, *menu,
        SystemControlsPanelModel{config->auto_sleep_enabled,
                                 config->auto_sleep_interval_index,
                                 language, levels},
        services);
  };
  panels.game_settings = [menu = &composition.menu_state,
                          settings = composition.game_settings,
                          language,
                          services = composition.services](const SDL_Rect &preview,
                                                            int first_y, int pitch,
                                                            int height) {
    DrawGameSettingsPanel(preview, first_y, pitch, height, *menu,
                          GameSettingsPanelModel{language, settings}, services);
  };
  panels.key_guide = [services = composition.services](const SDL_Rect &preview,
                                                       int first_y) {
    DrawKeyGuidePanel(preview, first_y, services);
  };
  panels.key_calibration = [menu = &composition.menu_state,
                            calibration = &composition.calibration,
                            services = composition.services](const SDL_Rect &preview,
                                                              int) {
    const int current = std::clamp(calibration->current, 0,
                                   static_cast<int>(KeyCalibrationButtons().size()) - 1);
    DrawKeyCalibrationPanel(
        preview,
        KeyCalibrationPanelModel{
            calibration->phase == KeyCalibrationPhase::Capturing,
            calibration->phase == KeyCalibrationPhase::Complete,
            calibration->phase == KeyCalibrationPhase::Failed,
            menu->panel_active,
            calibration->current,
            static_cast<int>(KeyCalibrationButtons().size()),
            InputManager::ButtonName(
                KeyCalibrationButtons()[static_cast<size_t>(current)]),
            calibration->status,
        },
        services);
  };
  panels.contributor_avatars = [renderer = composition.renderer,
                                assets = &composition.assets,
                                layout = &composition.layout,
                                avatars = &composition.avatars,
                                contributor = &composition.contributor_avatar,
                                menu = &composition.menu_state,
                                services = composition.services](const SDL_Rect &preview,
                                                                  int) {
    DrawContributorAvatarPanel(renderer, *assets, preview, layout->bottom_bar_y,
                               *avatars, *contributor, *menu, services);
  };
  panels.contact = [language, services = composition.services](const SDL_Rect &preview,
                                                               int first_y) {
    DrawContactPanel(preview, first_y, language, services);
  };
  panels.version_update = [language, update = composition.version_update,
                           active = composition.menu_state.panel_active,
                           services = composition.services](const SDL_Rect &preview,
                                                             int first_y) {
    DrawUpdatePanel(preview, first_y,
                    UpdatePanelModel{language, update.status, active}, services);
  };
  panels.exit_app = [language, services = composition.services](const SDL_Rect &preview,
                                                                int first_y) {
    DrawExitPanel(preview, first_y, language, services);
  };
  return panels;
}

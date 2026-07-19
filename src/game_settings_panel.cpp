#include "game_settings_panel.h"

#include <algorithm>
#include <array>

bool HandleGameSettingsPanelInput(
    const InputManager &input, SettingsRuntimeState &menu_state,
    GameSettingsState &state, const GameSettingsCallbacks &callbacks) {
  return HandleGameSettingsInput(input, menu_state, state, callbacks);
}

void DrawGameSettingsPanel(const SDL_Rect &preview, int first_row_y, int row_pitch,
                           int row_h, const SettingsRuntimeState &menu_state,
                           const GameSettingsPanelModel &model,
                           const MenuPanelDrawServices &services) {
  const SDL_Color text{236, 241, 247, 255};
  const SDL_Color muted{149, 164, 181, 255};
  const SDL_Color fill{29, 42, 57, 230};
  const SDL_Color selected_fill{41, 82, 113, 240};
  const SDL_Color selected_border{122, 201, 255, 255};
  const SDL_Color state_lit{63, 119, 158, 255};
  const int base_x = preview.x + 32;
  const int control_right = preview.x + preview.w - 56;
  const int button = 84;
  const int gap = 20;
  const int value_w = 330;
  const int left_arrow = control_right - button - gap - value_w - gap - button;
  const std::array<std::string, 5> labels = {{
      LocalizedGameSettingsLabel(model.language_index, 0),
      LocalizedGameSettingsLabel(model.language_index, 1),
      LocalizedGameSettingsLabel(model.language_index, 2),
      LocalizedGameSettingsLabel(model.language_index, 3),
      LocalizedGameSettingsLabel(model.language_index, 4),
  }};
  const std::array<std::string, 5> values = {{
      LocalizedGameAspectLabel(model.language_index, model.state.aspect),
      LocalizedGameFilterLabel(model.language_index, model.state.filter),
      model.state.virtual_mouse ? "On" : "Off",
      std::to_string(model.state.mouse_speed),
      GameMouseAccelerationLabel(model.state.mouse_acceleration),
  }};
  services.draw_rect(SDL_Rect{preview.x + 36, first_row_y - 36, preview.w - 72, 2},
                     SDL_Color{66, 95, 124, 255}, true, 1);

  auto draw_button = [&](const SDL_Rect &rect, const std::string &caption, bool selected,
                         bool lit = false) {
    services.draw_rect(rect, lit ? state_lit : (selected ? selected_fill : fill), true, 1);
    services.draw_rect(rect, selected ? selected_border : muted, false, selected ? 2 : 1);
    if (services.draw_text_centered) {
      services.draw_text_centered(caption, rect, text, MenuPanelTextStyle::Menu);
    }
  };
  for (int row = 0; row < 5; ++row) {
    const int y = first_row_y + row * row_pitch;
    if (services.draw_text) {
      services.draw_text(labels[static_cast<size_t>(row)], base_x,
                         y + std::max(0, (row_h - 56) / 2), text,
                         MenuPanelTextStyle::Menu);
    }
    const int control_y = y + (row_h - button) / 2;
    if (row == 2) {
      const int toggle_gap = 28;
      const int wide = (control_right - left_arrow - toggle_gap) / 2;
      const int off_x = control_right - wide;
      const int on_x = left_arrow;
      draw_button(SDL_Rect{on_x, control_y, wide, button}, u8"开",
                  menu_state.panel_active && menu_state.panel_focus == row &&
                      menu_state.panel_button == 0,
                  model.state.virtual_mouse);
      draw_button(SDL_Rect{off_x, control_y, wide, button}, u8"关",
                  menu_state.panel_active && menu_state.panel_focus == row &&
                      menu_state.panel_button == 1,
                  !model.state.virtual_mouse);
      continue;
    }
    draw_button(SDL_Rect{left_arrow, control_y, button, button}, row >= 3 ? "-" : "<",
                menu_state.panel_active && menu_state.panel_focus == row &&
                    menu_state.panel_button == 0);
    draw_button(SDL_Rect{control_right - button, control_y, button, button}, row >= 3 ? "+" : ">",
                menu_state.panel_active && menu_state.panel_focus == row &&
                    menu_state.panel_button == 1);
    if (services.draw_text_centered) {
      services.draw_text_centered(values[static_cast<size_t>(row)],
                                  SDL_Rect{left_arrow + button + gap, control_y, value_w, button},
                                  text, MenuPanelTextStyle::Menu);
    }
  }
}

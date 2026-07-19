#include "system_controls_panel.h"

#include "app_language.h"

#include <algorithm>
#include <array>

bool HandleSystemControlsPanelInput(
    const InputManager &input, SettingsRuntimeState &menu_state,
    const std::function<void(int, int)> &adjust_setting) {
  constexpr int kRows = 7;
  if (input.IsJustPressed(Button::B) || input.IsJustPressed(Button::Menu)) {
    menu_state.panel_active = false;
    menu_state.panel_focus = 0;
    menu_state.panel_button = 0;
    return true;
  }
  if (input.IsJustPressed(Button::Up) || input.IsRepeated(Button::Up)) {
    menu_state.panel_focus = (menu_state.panel_focus + kRows - 1) % kRows;
    if (menu_state.panel_focus >= 5) menu_state.panel_button = 1;
  }
  if (input.IsJustPressed(Button::Down) || input.IsRepeated(Button::Down)) {
    menu_state.panel_focus = (menu_state.panel_focus + 1) % kRows;
    if (menu_state.panel_focus >= 5) menu_state.panel_button = 1;
  }

  const bool dual = menu_state.panel_focus <= 4;
  if (dual && input.IsJustPressed(Button::Left)) menu_state.panel_button = 0;
  if (dual && input.IsJustPressed(Button::Right)) menu_state.panel_button = 1;
  int delta = 0;
  if (dual && (input.IsJustPressed(Button::Left) || input.IsRepeated(Button::Left))) delta = -1;
  else if (dual && (input.IsJustPressed(Button::Right) || input.IsRepeated(Button::Right))) delta = 1;
  else if (input.IsJustPressed(Button::A)) delta = menu_state.panel_button == 1 ? 1 : -1;
  if (delta != 0 && adjust_setting) adjust_setting(menu_state.panel_focus, delta);
  return true;
}

void DrawSystemControlsPanel(const SDL_Rect &preview, int first_row_y, int row_pitch,
                             int row_h, const SettingsRuntimeState &menu_state,
                             const SystemControlsPanelModel &model,
                             const MenuPanelDrawServices &services) {
  const std::array<std::string, 7> labels = {{
      LocalizedAppText(model.language_index, AppTextId::SystemKeySound),
      LocalizedAppText(model.language_index, AppTextId::SystemBrightness),
      LocalizedAppText(model.language_index, AppTextId::SystemAutoSleep),
      LocalizedAppText(model.language_index, AppTextId::SystemSleepTimer),
      LocalizedAppText(model.language_index, AppTextId::SystemLanguage),
      LocalizedAppText(model.language_index, AppTextId::SystemClearCache),
      LocalizedAppText(model.language_index, AppTextId::SystemClearHistory),
  }};
  const SDL_Color text{236, 241, 247, 255};
  const SDL_Color muted{149, 164, 181, 255};
  const SDL_Color fill{29, 42, 57, 230};
  const SDL_Color selected_fill{41, 82, 113, 240};
  const SDL_Color selected_border{122, 201, 255, 255};
  const SDL_Color active{122, 201, 255, 255};
  const SDL_Color state_lit{63, 119, 158, 255};
  const int button_w = 84;
  const int button_h = 84;
  const int long_w = 276;
  const int gap = 28;
  const int base_x = preview.x + 32;
  const int control_right = preview.x + preview.w - 56;
  int label_w = 0;
  for (const std::string &label : labels) {
    int w = 0;
    int h = 0;
    if (services.measure_text) services.measure_text(label, MenuPanelTextStyle::Menu, w, h);
    label_w = std::max(label_w, w);
  }
  const int control_left = base_x + label_w + 36;
  services.draw_rect(SDL_Rect{preview.x + 36, first_row_y - 36, preview.w - 72, 2},
                     SDL_Color{66, 95, 124, 255}, true, 1);

  auto draw_label = [&](int row) {
    if (services.draw_text) {
      services.draw_text(labels[static_cast<size_t>(row)], base_x,
                         first_row_y + row * row_pitch + std::max(0, (row_h - 56) / 2),
                         text, MenuPanelTextStyle::Menu);
    }
  };
  auto draw_button = [&](const SDL_Rect &rect, const std::string &caption, bool selected,
                         bool lit = false) {
    services.draw_rect(rect, lit ? state_lit : (selected ? selected_fill : fill), true, 1);
    services.draw_rect(rect, selected ? selected_border : muted, false, selected ? 2 : 1);
    if (services.draw_text_centered) {
      services.draw_text_centered(caption, rect, text, MenuPanelTextStyle::Menu);
    }
  };

  for (int row = 0; row < 2; ++row) {
    draw_label(row);
    const int cy = first_row_y + row * row_pitch + row_h / 2;
    const int minus_x = control_left;
    const int plus_x = control_right - button_w;
    const int meter_x = minus_x + button_w + gap;
    const int meter_w = std::max(70, plus_x - gap - meter_x);
    const SDL_Rect minus{minus_x, cy - button_h / 2, button_w, button_h};
    const SDL_Rect plus{plus_x, cy - button_h / 2, button_w, button_h};
    draw_button(minus, "-", menu_state.panel_active && menu_state.panel_focus == row &&
                                menu_state.panel_button == 0);
    draw_button(plus, "+", menu_state.panel_active && menu_state.panel_focus == row &&
                               menu_state.panel_button == 1);
    const SystemControlValue &value = row == 0 ? model.levels.volume : model.levels.brightness;
    const SDL_Rect meter{meter_x, cy - button_h / 2, meter_w, button_h};
    services.draw_rect(meter, SDL_Color{62, 77, 92, 255}, true, 1);
    const int max_level = std::max(1, value.max_level);
    const int level = value.available ? std::clamp(value.level, 0, max_level) : 0;
    const int fill_w = meter.w * level / max_level;
    if (fill_w > 0) {
      services.draw_rect(SDL_Rect{meter.x, meter.y, fill_w, meter.h}, active, true, 1);
    }
    services.draw_rect(meter, muted, false, 1);
  }

  draw_label(2);
  const int row2_y = first_row_y + row_pitch * 2 + (row_h - button_h) / 2;
  const int off_x = control_right - long_w;
  const int on_x = off_x - gap - long_w;
  draw_button(SDL_Rect{on_x, row2_y, long_w, button_h},
              LocalizedAppText(model.language_index, AppTextId::SystemOn),
              menu_state.panel_active && menu_state.panel_focus == 2 && menu_state.panel_button == 0,
              model.auto_sleep_enabled);
  draw_button(SDL_Rect{off_x, row2_y, long_w, button_h},
              LocalizedAppText(model.language_index, AppTextId::SystemOff),
              menu_state.panel_active && menu_state.panel_focus == 2 && menu_state.panel_button == 1,
              !model.auto_sleep_enabled);

  const int selector_w = 340;
  const int selector_left = control_right - button_w - 20 - selector_w - 20 - button_w;
  auto draw_selector = [&](int row, const std::string &value) {
    draw_label(row);
    const int y = first_row_y + row_pitch * row + (row_h - button_h) / 2;
    draw_button(SDL_Rect{selector_left, y, button_w, button_h}, "<",
                menu_state.panel_active && menu_state.panel_focus == row && menu_state.panel_button == 0);
    draw_button(SDL_Rect{control_right - button_w, y, button_w, button_h}, ">",
                menu_state.panel_active && menu_state.panel_focus == row && menu_state.panel_button == 1);
    if (services.draw_text_centered) {
      services.draw_text_centered(value,
                                  SDL_Rect{selector_left + button_w + 20, y, selector_w, button_h},
                                  text, MenuPanelTextStyle::Menu);
    }
  };
  draw_selector(3, LocalizedSleepIntervalLabel(model.language_index,
                                                model.auto_sleep_interval_index));
  draw_selector(4, SystemLanguageDisplayLabel(model.language_index));

  for (int row = 5; row <= 6; ++row) {
    draw_label(row);
    const int y = first_row_y + row_pitch * row + (row_h - button_h) / 2;
    draw_button(SDL_Rect{control_right - long_w, y, long_w, button_h},
                LocalizedAppText(model.language_index, AppTextId::SystemClear),
                menu_state.panel_active && menu_state.panel_focus == row);
  }
}

#include "key_calibration_panel.h"

#include <algorithm>

void DrawKeyCalibrationPanel(const SDL_Rect &preview,
                             const KeyCalibrationPanelModel &model,
                             const MenuPanelDrawServices &services) {
  const int center_y = preview.y + preview.h / 2;
  if (model.capturing) {
    const int current = std::clamp(model.current, 0, std::max(0, model.total - 1));
    if (services.draw_text_centered) {
      services.draw_text_centered(u8"按键校准",
                                  SDL_Rect{preview.x + 40, preview.y + 135, preview.w - 80, 70},
                                  SDL_Color{240, 246, 255, 255}, MenuPanelTextStyle::Menu);
      services.draw_text_centered(u8"进度 " + std::to_string(current) + " / " +
                                      std::to_string(model.total) + u8" 个按键",
                                  SDL_Rect{preview.x + 40, center_y - 180, preview.w - 80, 60},
                                  SDL_Color{149, 164, 181, 255}, MenuPanelTextStyle::Body);
      services.draw_text_centered(u8"请按下",
                                  SDL_Rect{preview.x + 40, center_y - 80, preview.w - 80, 60},
                                  SDL_Color{191, 221, 247, 255}, MenuPanelTextStyle::Body);
      services.draw_text_centered(model.current_button,
                                  SDL_Rect{preview.x + 40, center_y - 10, preview.w - 80, 100},
                                  SDL_Color{245, 248, 255, 255}, MenuPanelTextStyle::Title);
    }
    return;
  }

  const SDL_Rect action{preview.x + (preview.w - 460) / 2, center_y - 50, 460, 100};
  const std::string title = model.complete || model.failed ? model.status : u8"按键校准";
  if (services.draw_text_centered) {
    services.draw_text_centered(title,
                                SDL_Rect{preview.x + 60, action.y - 110, preview.w - 120, 76},
                                model.failed ? SDL_Color{255, 184, 164, 255}
                                             : SDL_Color{240, 246, 255, 255},
                                MenuPanelTextStyle::Menu);
  }
  services.draw_rect(action, model.panel_active ? SDL_Color{41, 82, 113, 255}
                                                : SDL_Color{29, 42, 57, 255},
                     true, 1);
  services.draw_rect(action, SDL_Color{122, 201, 255, 255}, false, 2);
  if (services.draw_text_centered) {
    services.draw_text_centered(model.complete ? u8"按 A 重新校准" : u8"按 A 开始校准",
                                action, SDL_Color{245, 248, 255, 255},
                                MenuPanelTextStyle::Menu);
  }
}

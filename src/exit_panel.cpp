#include "exit_panel.h"

#include "app_language.h"
#include "settings_runtime.h"

void DrawExitPanel(const SDL_Rect &preview, int first_row_y, int language_index,
                   const MenuPanelDrawServices &services) {
  if (services.draw_text) {
    services.draw_text(SettingLabel(SettingId::ExitApp, language_index), preview.x + 64,
                       first_row_y - 104, SDL_Color{240, 246, 255, 255},
                       MenuPanelTextStyle::Menu);
  }
  services.draw_rect(SDL_Rect{preview.x + 36, first_row_y - 36, preview.w - 72, 2},
                     SDL_Color{66, 95, 124, 255}, true, 1);
  if (services.draw_text_centered) {
    services.draw_text_centered(LocalizedAppText(language_index, AppTextId::ExitHint),
                                SDL_Rect{preview.x + 40, first_row_y + 180, preview.w - 80, 100},
                                SDL_Color{230, 236, 248, 255}, MenuPanelTextStyle::Menu);
  }
}

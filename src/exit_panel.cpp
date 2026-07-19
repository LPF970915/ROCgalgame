#include "exit_panel.h"

#include "app_language.h"
#include "settings_runtime.h"

void DrawExitPanel(const SDL_Rect &preview, int first_row_y, int language_index,
                   const MenuPanelDrawServices &services) {
  (void)first_row_y;
  if (services.draw_text_centered) {
    services.draw_text_centered(LocalizedAppText(language_index, AppTextId::ExitHint),
                                preview, SDL_Color{240, 246, 255, 255},
                                MenuPanelTextStyle::Title);
  }
}

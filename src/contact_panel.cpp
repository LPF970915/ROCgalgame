#include "contact_panel.h"

#include "app_language.h"

void DrawContactPanel(const SDL_Rect &preview, int first_row_y, int language_index,
                      const MenuPanelDrawServices &services) {
  if (!services.draw_text_centered) return;
  services.draw_text_centered(
      LocalizedAppText(language_index, AppTextId::ContactRewardHint),
      SDL_Rect{preview.x + 40, first_row_y - 16, preview.w - 80, 86},
      SDL_Color{240, 246, 255, 255}, MenuPanelTextStyle::Body);
}

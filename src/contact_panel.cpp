#include "contact_panel.h"

#include "app_language.h"

void DrawContactPanel(const SDL_Rect &preview, const LayoutMetrics &layout, int language_index,
                      const MenuPanelDrawServices &services) {
  if (!services.draw_text_centered) return;
  const int safe_y = std::max(preview.y + 88, layout.top_bar_y + layout.top_bar_h + 48);
  services.draw_text_centered(
      LocalizedAppText(language_index, AppTextId::ContactRewardHint),
      SDL_Rect{preview.x + 16, safe_y, preview.w - 32, 92},
      SDL_Color{240, 246, 255, 255}, MenuPanelTextStyle::Menu);
}

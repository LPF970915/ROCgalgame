#include "update_panel.h"

#include "app_language.h"

void DrawUpdatePanel(const SDL_Rect &preview, int first_row_y,
                     const UpdatePanelModel &model,
                     const MenuPanelDrawServices &services) {
  if (services.draw_text) {
    services.draw_text(LocalizedAppText(model.language_index, AppTextId::SettingVersionUpdate),
                       preview.x + 64, first_row_y - 104,
                       SDL_Color{240, 246, 255, 255}, MenuPanelTextStyle::Menu);
  }
  services.draw_rect(SDL_Rect{preview.x + 36, first_row_y - 36, preview.w - 72, 2},
                     SDL_Color{66, 95, 124, 255}, true, 1);
  if (services.draw_text) {
    services.draw_text(LocalizedAppText(model.language_index, AppTextId::VersionCurrentVersion),
                       preview.x + 88, first_row_y + 80,
                       SDL_Color{149, 164, 181, 255}, MenuPanelTextStyle::Body);
  }
  if (services.draw_text_right) {
    services.draw_text_right(u8"开发版", preview.x + preview.w - 88, first_row_y + 80,
                             SDL_Color{236, 241, 247, 255}, MenuPanelTextStyle::Body);
  }
  std::string status = LocalizedAppText(model.language_index, AppTextId::VersionPressAToCheck);
  if (model.status == VersionUpdateStatus::Unconfigured) status = u8"更新地址尚未配置";
  else if (model.status == VersionUpdateStatus::Checking) status = u8"更新接口已预留，等待接入发版源";
  if (services.draw_text_centered) {
    services.draw_text_centered(status,
                                SDL_Rect{preview.x + 60, first_row_y + 230, preview.w - 120, 80},
                                SDL_Color{230, 236, 248, 255}, MenuPanelTextStyle::Menu);
  }
  const SDL_Rect action{preview.x + (preview.w - 520) / 2, first_row_y + 380, 520, 100};
  services.draw_rect(action, model.panel_active ? SDL_Color{41, 82, 113, 255}
                                               : SDL_Color{29, 42, 57, 255},
                     true, 1);
  services.draw_rect(action, SDL_Color{122, 201, 255, 255}, false, 2);
  if (services.draw_text_centered) {
    services.draw_text_centered(
        LocalizedAppText(model.language_index, AppTextId::VersionCheckAndUpdate), action,
        SDL_Color{245, 248, 255, 255}, MenuPanelTextStyle::Menu);
  }
}

#include "update_panel.h"

#include "app_language.h"

#include <algorithm>

void DrawUpdatePanel(const SDL_Rect &preview, int first_row_y,
                     const UpdatePanelModel &model,
                     const MenuPanelDrawServices &services) {
  (void)first_row_y;
  const int content_top = preview.y + std::max(0, (preview.h - 324) / 2);
  const int line1_y = content_top + 36;
  const int line2_y = content_top + 152;
  const int line3_y = content_top + 272;
  const SDL_Color text{236, 241, 247, 255};
  if (services.draw_text_centered) {
    services.draw_text_centered(
        LocalizedAppText(model.language_index, AppTextId::VersionCurrentVersion) +
            std::string(" ") + model.current_version,
        SDL_Rect{preview.x, line1_y - 48, preview.w, 96}, text,
        MenuPanelTextStyle::Title);
  }
  std::string status = LocalizedAppText(model.language_index, AppTextId::VersionPressAToCheck);
  if (model.status == VersionUpdateStatus::Unconfigured) status = u8"更新地址尚未配置";
  else if (model.status == VersionUpdateStatus::NoNetwork) {
    status = LocalizedAppText(model.language_index, AppTextId::VersionNoNetwork);
  } else if (model.status == VersionUpdateStatus::Downloading) {
    status = LocalizedAppText(model.language_index, AppTextId::VersionDownloading);
    if (model.download_progress_pct > 0) status += " " + std::to_string(model.download_progress_pct) + "%";
  } else if (model.status == VersionUpdateStatus::Downloaded) {
    status = std::string(LocalizedAppText(model.language_index, AppTextId::VersionDownloadedPackage)) +
             " " + LocalizedAppText(model.language_index, AppTextId::VersionRestartToInstall);
  } else if (model.status == VersionUpdateStatus::UpToDate) {
    status = LocalizedAppText(model.language_index, AppTextId::VersionAlreadyLatest);
  } else if (model.status == VersionUpdateStatus::DownloadFailed) {
    status = LocalizedAppText(model.language_index, AppTextId::VersionDownloadFailed);
  }
  const SDL_Rect action{preview.x + (preview.w - 352) / 2, line2_y - 42, 352, 84};
  services.draw_rect(action, model.panel_active ? SDL_Color{41, 82, 113, 255}
                                               : SDL_Color{29, 42, 57, 255},
                     true, 1);
  services.draw_rect(action, SDL_Color{122, 201, 255, 255}, false, 2);
  if (services.draw_text_centered) {
    services.draw_text_centered(
        LocalizedAppText(model.language_index, AppTextId::VersionCheckAndUpdate), action,
        text, MenuPanelTextStyle::Menu);
    services.draw_text_centered(status,
                                SDL_Rect{preview.x, line3_y - 46, preview.w, 92},
                                SDL_Color{155, 168, 182, 255}, MenuPanelTextStyle::Menu);
  }
  if (model.status == VersionUpdateStatus::Downloading && model.download_progress_pct > 0) {
    const SDL_Rect track{preview.x + (preview.w - 520) / 2, line3_y + 44, 520, 18};
    services.draw_rect(track, SDL_Color{29, 42, 57, 255}, true, 1);
    const int fill_w = std::clamp(model.download_progress_pct, 0, 100) * track.w / 100;
    if (fill_w > 0) {
      services.draw_rect(SDL_Rect{track.x, track.y, fill_w, track.h},
                         SDL_Color{122, 201, 255, 255}, true, 1);
    }
  }
}

#include "settings_runtime.h"

#include "app_language.h"

#include <algorithm>
#include <cmath>

namespace {
int ScalePx(float scale, int value) {
  return std::max(1, static_cast<int>(std::round(value * std::max(0.1f, scale))));
}
}  // namespace

std::string SettingLabel(SettingId id, int language_index) {
  switch (id) {
    case SettingId::KeyGuide:
      return LocalizedAppText(language_index, AppTextId::SettingKeyGuide);
    case SettingId::KeyCalibration:
      return LocalizedAppText(language_index, AppTextId::SettingKeyCalibration);
    case SettingId::SystemControls:
      return LocalizedAppText(language_index, AppTextId::SettingSystemControls);
    case SettingId::GameSettings:
      if (language_index == 0) return u8"游戏设置";
      if (language_index == 1) return u8"遊戲設定";
      return "Game Settings";
    case SettingId::ContributorAvatars:
      return LocalizedAppText(language_index, AppTextId::SettingContributorAvatars);
    case SettingId::ContactMe:
      return LocalizedAppText(language_index, AppTextId::SettingContactMe);
    case SettingId::VersionUpdate:
      return LocalizedAppText(language_index, AppTextId::SettingVersionUpdate);
    case SettingId::ExitApp:
      return LocalizedAppText(language_index, AppTextId::SettingExitApp);
  }
  return {};
}

const char *SettingPreviewAsset(SettingId id) {
  return id == SettingId::ContactMe ? "Menu_Contact Me.png" : "Menu_Default.png";
}

void HandleSettingsInput(SettingsRuntimeInputDeps &deps) {
  SettingsRuntimeState &state = deps.state;
  if (!deps.actions.animations_enabled) state.anim.Snap(state.closing ? 0.0f : 1.0f);
  state.anim.Update(deps.dt);

  if (state.closing && state.anim.Value() <= 0.0001f && !state.anim.IsAnimating()) {
    if (deps.actions.on_close) deps.actions.on_close();
    return;
  }

  if (!state.close_armed) {
    const bool held = deps.input.IsPressed(Button::Start) || deps.input.IsPressed(Button::Select) ||
                      deps.input.IsPressed(Button::Menu);
    if (!held) state.close_armed = true;
  }

  if (state.closing || state.items.empty()) return;
  state.selected = std::clamp(state.selected, 0, static_cast<int>(state.items.size()) - 1);
  const SettingId selected = state.items[static_cast<size_t>(state.selected)];

  if (state.panel_active) {
    if (HandleSelectedSettingsPanelInput(selected, state, deps.actions.panel_handlers)) return;
    if (deps.input.IsJustPressed(Button::B) || deps.input.IsJustPressed(Button::Menu)) {
      state.panel_active = false;
      state.panel_focus = 0;
      state.panel_button = 0;
    }
    return;
  }

  if (state.close_armed && state.toggle_guard <= 0.0f &&
      (deps.input.IsJustPressed(Button::B) || deps.actions.menu_toggle_request)) {
    if (deps.actions.animations_enabled) {
      state.anim.AnimateTo(0.0f, 0.16f, animation::Ease::InOutCubic);
    } else {
      state.anim.Snap(0.0f);
    }
    state.closing = true;
    return;
  }

  if (deps.input.IsJustPressed(Button::Up) || deps.input.IsRepeated(Button::Up)) {
    state.selected = (state.selected - 1 + static_cast<int>(state.items.size())) %
                     static_cast<int>(state.items.size());
  } else if (deps.input.IsJustPressed(Button::Down) || deps.input.IsRepeated(Button::Down)) {
    state.selected = (state.selected + 1) % static_cast<int>(state.items.size());
  } else if (deps.input.IsJustPressed(Button::A) || deps.input.IsJustPressed(Button::Right)) {
    if (selected == SettingId::ExitApp) {
      if (deps.actions.on_exit_app) deps.actions.on_exit_app();
      return;
    }
    state.panel_active = true;
    state.panel_focus = 0;
    state.panel_button = 0;
    if (deps.actions.on_open_panel) deps.actions.on_open_panel(selected, state);
  }
}

void DrawSettingsRuntime(SettingsRuntimeRenderDeps &deps) {
  if (!deps.renderer || deps.state.items.empty()) return;
  const float progress = deps.state.anim.Value();
  const float eased = animation::ApplyEase(animation::Ease::OutCubic, progress);
  const int language = SystemLanguageIndexFromConfigValue(deps.cfg.system_language);
  const int menu_y = deps.layout.settings_y_offset;
  const int menu_h = std::max(0, deps.layout.screen_h - menu_y);
  const int menu_w = deps.layout.settings_sidebar_w;
  const int x = static_cast<int>(-menu_w + menu_w * eased);
  const int preview_x = x + menu_w;
  const SDL_Rect preview{preview_x, menu_y, std::max(0, deps.layout.screen_w - preview_x), menu_h};

  deps.services.draw_rect(0, 0, deps.layout.screen_w, deps.layout.screen_h,
                          SDL_Color{0, 0, 0, static_cast<Uint8>(100 * eased)}, true);
  const SettingId selected = deps.state.items[static_cast<size_t>(std::clamp(
      deps.state.selected, 0, static_cast<int>(deps.state.items.size()) - 1))];
  if (TextureHandle *texture = deps.services.get_texture(SettingPreviewAsset(selected));
      texture && texture->texture && deps.services.draw_texture_fit) {
    deps.services.draw_texture_fit(texture, preview);
  } else {
    deps.services.draw_rect(preview.x, preview.y, preview.w, preview.h,
                            SDL_Color{14, 20, 30, 240}, true);
  }

  deps.services.draw_rect(x, menu_y, menu_w, menu_h, SDL_Color{24, 34, 46, 255}, true);
  deps.services.draw_rect(x + menu_w - 1, menu_y, 1, menu_h,
                          SDL_Color{82, 125, 158, 255}, true);

  const float scale = deps.layout.ui_scale;
  const int margin_x = ScalePx(scale, 18);
  int item_h = ScalePx(scale, 42);
  int item_pitch = ScalePx(scale, 58);
  const int text_left = x + ScalePx(scale, 36);
  int divider_y = menu_y + ScalePx(scale, 68) + deps.layout.settings_content_offset_y;
  if (deps.services.get_title_text_texture) {
    const SDL_Color title_color{240, 246, 255, 255};
    TextCacheEntry *title = deps.services.get_title_text_texture(
        LocalizedAppText(language, AppTextId::MenuTitle), title_color);
    if (title && title->texture) {
      const int title_y = menu_y + ScalePx(scale, 10) + deps.layout.settings_content_offset_y;
      SDL_Rect dst{x + std::max(0, (menu_w - title->w) / 2), title_y, title->w, title->h};
      SDL_RenderCopy(deps.renderer, title->texture, nullptr, &dst);
      divider_y = title_y + title->h + ScalePx(scale, 12);
    }
  }
  deps.services.draw_rect(x + ScalePx(scale, 12), divider_y,
                          menu_w - ScalePx(scale, 24), ScalePx(scale, 1),
                          SDL_Color{66, 95, 124, 255}, true);

  const SDL_Color item_color{230, 236, 248, 255};
  if (deps.services.get_text_texture) {
    int max_h = 0;
    for (SettingId id : deps.state.items) {
      if (TextCacheEntry *entry = deps.services.get_text_texture(SettingLabel(id, language), item_color)) {
        max_h = std::max(max_h, entry->h);
      }
    }
    item_h = std::max(item_h, max_h + ScalePx(scale, 18));
    item_pitch = std::max(item_pitch, item_h + ScalePx(scale, 16));
  }

  const int first_y = divider_y + ScalePx(scale, 18);
  const int visible_bottom = deps.layout.bottom_bar_y;
  const int selected_top = first_y + deps.state.selected * item_pitch;
  const int selected_bottom = selected_top + item_h;
  int scroll_y = selected_bottom > visible_bottom ? selected_bottom - visible_bottom : 0;
  scroll_y = std::max(0, scroll_y);

  SDL_Rect previous_clip{};
  const SDL_bool had_clip = SDL_RenderIsClipEnabled(deps.renderer);
  if (had_clip == SDL_TRUE) SDL_RenderGetClipRect(deps.renderer, &previous_clip);
  SDL_Rect clip{x, first_y - ScalePx(scale, 12), menu_w,
                std::max(0, visible_bottom - first_y + ScalePx(scale, 12))};
  SDL_RenderSetClipRect(deps.renderer, &clip);
  for (size_t i = 0; i < deps.state.items.size(); ++i) {
    const int y = first_y + static_cast<int>(i) * item_pitch - scroll_y;
    const bool focused = static_cast<int>(i) == deps.state.selected;
    const SDL_Color fill = focused ? SDL_Color{63, 119, 158, 255}
                                   : SDL_Color{57, 73, 96, 214};
    deps.services.draw_rect(x + margin_x, y, menu_w - margin_x * 2, item_h, fill, true);
    if (focused) {
      deps.services.draw_rect(x + margin_x, y, ScalePx(scale, 4), item_h,
                              SDL_Color{139, 214, 255, 255}, true);
      deps.services.draw_rect(x + margin_x - ScalePx(scale, 1), y - ScalePx(scale, 1),
                              menu_w - margin_x * 2 + ScalePx(scale, 2),
                              item_h + ScalePx(scale, 2), SDL_Color{85, 152, 198, 208}, false);
    }
    if (deps.services.get_text_texture) {
      TextCacheEntry *entry = deps.services.get_text_texture(
          SettingLabel(deps.state.items[i], language), item_color);
      if (entry && entry->texture) {
        SDL_Rect dst{text_left, y + std::max(0, (item_h - entry->h) / 2), entry->w, entry->h};
        SDL_RenderCopy(deps.renderer, entry->texture, nullptr, &dst);
      }
    }
  }
  SDL_RenderSetClipRect(deps.renderer, had_clip == SDL_TRUE ? &previous_clip : nullptr);

  DrawSelectedSettingsPanel(selected, deps, preview, first_y, item_pitch, item_h);
  if (deps.services.draw_chrome) deps.services.draw_chrome();
}

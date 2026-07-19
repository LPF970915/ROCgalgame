#include "app_loop.h"

#include "app_bootstrap.h"
#include "app_composition.h"
#include "app_environment.h"
#include "app_layout.h"
#include "app_language.h"
#include "app_runtime.h"
#include "app_services.h"
#include "app_shell.h"
#include "app_stores.h"
#include "audio_runtime.h"
#include "config.h"
#include "cover_cache_runtime.h"
#include "cover_service.h"
#include "game_library.h"
#include "game_library_service.h"
#include "game_core_registry.h"
#include "game_launch_service.h"
#include "game_settings_runtime.h"
#include "input_manager.h"
#include "key_calibration_runtime.h"
#include "krkr_core_adapter.h"
#include "lid_power_control.h"
#include "menu_scene.h"
#include "ons_core_adapter.h"
#include "power_lifecycle.h"
#include "screen_profile.h"
#include "settings_composition.h"
#include "shelf_runtime.h"
#include "shelf_scene.h"
#include "status_bar_runtime.h"
#include "system_controls.h"
#include "system_settings_runtime.h"
#include "system_status.h"
#include "ui_assets.h"
#include "ui_text_cache.h"
#include "version_update_runtime.h"
#include "volume_overlay.h"

#include <SDL.h>
#ifdef HAVE_SDL2_TTF
#include <SDL_ttf.h>
#endif

#include <algorithm>
#include <array>
#include <cmath>
#include <cstdlib>
#include <filesystem>
#include <fstream>
#include <iomanip>
#include <iostream>
#include <iterator>
#include <memory>
#include <sstream>
#include <string>
#include <utility>
#include <vector>

namespace {
const std::vector<SettingId> kMenuItems = {
    SettingId::KeyGuide,
    SettingId::KeyCalibration,
    SettingId::SystemControls,
    SettingId::GameSettings,
    SettingId::ContributorAvatars,
    SettingId::ContactMe,
    SettingId::VersionUpdate,
    SettingId::ExitApp,
};

SettingsRuntimeState MakeInitialMenuState() {
  SettingsRuntimeState state;
  state.items = kMenuItems;
  return state;
}

struct AppState {
  ConfigStore config_store;
  AppConfig &config;
  ScreenProfile screen_profile;
  InputProfile input_profile = InputProfile::DesktopDefault;
  AppPlatformEnvironment platform_environment;
  LayoutMetrics layout;
  SDL_Window *window = nullptr;
  SDL_Renderer *renderer = nullptr;
#ifdef HAVE_SDL2_TTF
  TTF_Font *title_font = nullptr;
  TTF_Font *font = nullptr;
  TTF_Font *menu_font = nullptr;
  TTF_Font *small_font = nullptr;
  TTF_Font *micro_font = nullptr;
#endif
  UiAssets assets;
  UiTextCache text_cache;
  InputManager input;
  SfxBank sfx;
  bool sfx_ready = false;
  SystemControlService system_controls;
  SystemControlLevels system_levels;
  VolumeController volume_controller;
  AppUiState ui_state;
  SystemSettingsCallbacks system_settings_callbacks;
  LidPowerController lid_power_controller;
  std::unique_ptr<PowerLifecycleController> power_lifecycle;
  SystemStatusMonitor system_status;
  AppInputDevices input_devices;
  GameLibraryService game_library;
  ShelfRuntimeState shelf_runtime;
  ShelfScene shelf_scene;
  CoverCacheRuntime cover_cache;
  bool running = true;
  bool launch_pending = false;
  GameEntry pending_game;
  bool capture_done = false;
  bool menu_open = false;
  MenuScene menu_scene;
  SettingsRuntimeState menu_state = MakeInitialMenuState();
  std::vector<ContributorAvatarEntry> avatars;
  ContributorAvatarState contributor_avatar_state;
  KeyCalibrationState calibration;
  VersionUpdateState version_update_state;
  GameSettingsState game_settings_state;
  GameSettingsCallbacks game_settings_callbacks;
  std::string message;
  Uint32 message_until = 0;

  AppState()
      : config(config_store.Mutable()),
#ifdef _WIN32
        system_controls(false), volume_controller(system_controls, system_levels, false) {}
#else
        system_controls(true), volume_controller(system_controls, system_levels, true) {}
#endif
};

int LanguageIndex(const AppState &app) { return SystemLanguageIndexFromConfigValue(app.config.system_language); }

SDL_Color Color(Uint8 r, Uint8 g, Uint8 b, Uint8 a = 255) { return SDL_Color{r, g, b, a}; }

constexpr std::array<Uint32, 6> kAutoSleepIntervalsMs = {{
    30u * 1000u,
    60u * 1000u,
    3u * 60u * 1000u,
    5u * 60u * 1000u,
    10u * 60u * 1000u,
    30u * 60u * 1000u,
}};

Uint32 AutoSleepIntervalMs(int index) {
  return kAutoSleepIntervalsMs[static_cast<size_t>(std::clamp(index, 0, 5))];
}

bool IsUserInputEvent(const SDL_Event &event) {
  if (event.type == SDL_CONTROLLERAXISMOTION) return std::abs(static_cast<int>(event.caxis.value)) > 12000;
  if (event.type == SDL_JOYAXISMOTION) return std::abs(static_cast<int>(event.jaxis.value)) > 12000;
  return event.type == SDL_KEYDOWN || event.type == SDL_KEYUP ||
         event.type == SDL_CONTROLLERBUTTONDOWN || event.type == SDL_CONTROLLERBUTTONUP ||
         event.type == SDL_JOYBUTTONDOWN || event.type == SDL_JOYBUTTONUP ||
         event.type == SDL_JOYHATMOTION;
}

void LoadAvatarEntries(AppState &app) {
  app.avatars.clear();
  const std::filesystem::path root = app.config.root / "ui" / "common";
  std::error_code ec;
  for (std::filesystem::directory_iterator it(root, std::filesystem::directory_options::skip_permission_denied, ec), end;
       it != end; it.increment(ec)) {
    if (ec) {
      ec.clear();
      continue;
    }
    if (!it->is_regular_file(ec) || ec) continue;
    const std::string filename = it->path().filename().u8string();
    if (ToLowerAscii(filename).rfind("avatar_", 0) != 0) continue;
    ContributorAvatarEntry avatar;
    avatar.path = it->path();
    avatar.filename = filename;
    const size_t marker = filename.find(u8"贡献值");
    if (marker != std::string::npos) {
      const std::string value = filename.substr(marker + std::string(u8"贡献值").size());
      avatar.contribution_is_max = ToLowerAscii(value).rfind("max", 0) == 0;
      if (!avatar.contribution_is_max) avatar.contribution = std::strtod(value.c_str(), nullptr);
    }
    app.avatars.push_back(std::move(avatar));
  }
  std::sort(app.avatars.begin(), app.avatars.end(), [](const ContributorAvatarEntry &a, const ContributorAvatarEntry &b) {
    if (a.contribution_is_max != b.contribution_is_max) return !a.contribution_is_max;
    if (!a.contribution_is_max && a.contribution != b.contribution) return a.contribution > b.contribution;
    return a.filename < b.filename;
  });
  auto first_max = std::find_if(app.avatars.begin(), app.avatars.end(), [](const ContributorAvatarEntry &entry) {
    return entry.contribution_is_max;
  });
  if (first_max != app.avatars.end()) {
    std::vector<ContributorAvatarEntry> max_entries(first_max, app.avatars.end());
    app.avatars.erase(first_max, app.avatars.end());
    const size_t insert_at = std::min<size_t>(3, app.avatars.size());
    app.avatars.insert(app.avatars.begin() + static_cast<std::ptrdiff_t>(insert_at),
                       std::make_move_iterator(max_entries.begin()), std::make_move_iterator(max_entries.end()));
  }
  app.contributor_avatar_state.focus_index = 0;
  for (int i = 0; i < static_cast<int>(app.avatars.size()); ++i) {
    if ((!app.config.selected_avatar.empty() && app.avatars[static_cast<size_t>(i)].filename == app.config.selected_avatar) ||
        (app.config.selected_avatar.empty() && app.avatars[static_cast<size_t>(i)].filename.find("BloodROC") != std::string::npos)) {
      app.contributor_avatar_state.focus_index = i;
      break;
    }
  }
  if (!app.avatars.empty() && app.config.selected_avatar.empty()) {
    app.config.selected_avatar = app.avatars[static_cast<size_t>(app.contributor_avatar_state.focus_index)].filename;
  }
  app.contributor_avatar_state.scroll_row =
      std::max(0, app.contributor_avatar_state.focus_index / 3 - 1);
}

TextureHandle *SelectedAvatarTexture(AppState &app) {
  if (app.avatars.empty()) return nullptr;
  for (const ContributorAvatarEntry &entry : app.avatars) {
    if (entry.filename == app.config.selected_avatar) return app.assets.LoadExternal(app.renderer, entry.path);
  }
  return app.assets.LoadExternal(app.renderer, app.avatars.front().path);
}

void ShowMessage(AppState &app, std::string text, Uint32 ms = 1800) {
  app.message = std::move(text);
  app.message_until = SDL_GetTicks() + ms;
}

void MarkConfigDirty(AppState &app) {
  app.config_store.MarkDirty(SDL_GetTicks());
}

void SaveSetting(AppState &app, const std::string &message) {
  MarkConfigDirty(app);
  ShowMessage(app, message);
}

void PlaySfx(AppState &app, SfxId id) {
  if (app.config.key_sound && app.sfx_ready) app.sfx.Play(id);
}

void ShowVolumeOverlay(AppState &app) {
  app.ui_state.volume_display_percent = std::clamp(app.config.system_volume_percent, 0, 100);
  app.ui_state.volume_display_until = SDL_GetTicks() + 1500;
}

bool AdjustVolume(AppState &app, int delta) {
  if (!app.system_settings_callbacks.adjust_volume) return false;
  return app.system_settings_callbacks.adjust_volume(delta, SDL_GetTicks());
}

bool IsValidUtf8(const std::string &text) {
  size_t i = 0;
  while (i < text.size()) {
    const unsigned char c = static_cast<unsigned char>(text[i]);
    size_t n = 0;
    if (c <= 0x7f) n = 1;
    else if ((c & 0xe0) == 0xc0) n = 2;
    else if ((c & 0xf0) == 0xe0) n = 3;
    else if ((c & 0xf8) == 0xf0) n = 4;
    else return false;
    if (i + n > text.size()) return false;
    for (size_t j = 1; j < n; ++j) {
      if ((static_cast<unsigned char>(text[i + j]) & 0xc0) != 0x80) return false;
    }
    i += n;
  }
  return true;
}

std::vector<size_t> Utf8Ends(const std::string &text) {
  std::vector<size_t> ends;
  size_t i = 0;
  while (i < text.size()) {
    const unsigned char c = static_cast<unsigned char>(text[i]);
    size_t n = 1;
    if ((c & 0xe0) == 0xc0) n = 2;
    else if ((c & 0xf0) == 0xe0) n = 3;
    else if ((c & 0xf8) == 0xf0) n = 4;
    if (i + n > text.size()) break;
    bool valid = true;
    for (size_t j = 1; j < n; ++j) valid = valid && ((static_cast<unsigned char>(text[i + j]) & 0xc0) == 0x80);
    if (!valid) break;
    i += n;
    ends.push_back(i);
  }
  return ends;
}

void Fill(SDL_Renderer *renderer, const SDL_Rect &rect, SDL_Color color) {
  SDL_SetRenderDrawBlendMode(renderer, SDL_BLENDMODE_BLEND);
  SDL_SetRenderDrawColor(renderer, color.r, color.g, color.b, color.a);
  SDL_RenderFillRect(renderer, &rect);
}

void Stroke(SDL_Renderer *renderer, const SDL_Rect &rect, SDL_Color color, int thickness = 1) {
  SDL_SetRenderDrawBlendMode(renderer, SDL_BLENDMODE_BLEND);
  SDL_SetRenderDrawColor(renderer, color.r, color.g, color.b, color.a);
  for (int i = 0; i < thickness; ++i) {
    SDL_Rect r{rect.x + i, rect.y + i, std::max(0, rect.w - i * 2), std::max(0, rect.h - i * 2)};
    SDL_RenderDrawRect(renderer, &r);
  }
}

#ifdef HAVE_SDL2_TTF
bool MeasureText(TTF_Font *font, const std::string &text, int &w, int &h) {
  w = 0;
  h = 0;
  if (!font || text.empty() || !IsValidUtf8(text)) return false;
  return TTF_SizeUTF8(font, text.c_str(), &w, &h) == 0 && w > 0 && h > 0 && w <= 4096 && h <= 512;
}

void DrawText(AppState &app, const std::string &text, int x, int y, SDL_Color color, TTF_Font *font = nullptr) {
  if (!font) font = app.font;
  int text_w = 0;
  int text_h = 0;
  if (!MeasureText(font, text, text_w, text_h)) return;
  TextCacheEntry *entry = app.text_cache.Get(app.renderer, font, text, color);
  if (!entry || !entry->texture) return;
  SDL_Rect dst{x, y, entry->w, entry->h};
  SDL_RenderCopy(app.renderer, entry->texture, nullptr, &dst);
}

void DrawTextRight(AppState &app, const std::string &text, int right, int y, SDL_Color color, TTF_Font *font = nullptr) {
  if (!font) font = app.font;
  int w = 0;
  int h = 0;
  if (!MeasureText(font, text, w, h)) return;
  DrawText(app, text, right - w, y, color, font);
}

void DrawTextCentered(AppState &app, const std::string &text, const SDL_Rect &rect, SDL_Color color,
                      TTF_Font *font = nullptr) {
  if (!font) font = app.font;
  int w = 0;
  int h = 0;
  if (!MeasureText(font, text, w, h)) return;
  DrawText(app, text, rect.x + std::max(0, (rect.w - w) / 2), rect.y + std::max(0, (rect.h - h) / 2), color, font);
}

std::string Ellipsize(TTF_Font *font, const std::string &text, int max_w) {
  if (!font || !IsValidUtf8(text)) return IsValidUtf8(text) ? text : "Untitled";
  int w = 0;
  int h = 0;
  if (TTF_SizeUTF8(font, text.c_str(), &w, &h) == 0 && w <= max_w) return text;
  const std::vector<size_t> ends = Utf8Ends(text);
  for (size_t count = ends.size(); count > 0; --count) {
    std::string candidate = text.substr(0, ends[count - 1]) + "...";
    if (TTF_SizeUTF8(font, candidate.c_str(), &w, &h) == 0 && w <= max_w) return candidate;
  }
  return "...";
}
#else
void DrawText(AppState &, const std::string &, int, int, SDL_Color, void * = nullptr) {}
void DrawTextRight(AppState &, const std::string &, int, int, SDL_Color, void * = nullptr) {}
void DrawTextCentered(AppState &, const std::string &, const SDL_Rect &, SDL_Color, void * = nullptr) {}
std::string Ellipsize(void *, const std::string &text, int) { return text; }
#endif

void DrawTextureNative(SDL_Renderer *renderer, TextureHandle *texture, int x, int y) {
  if (!texture || !texture->texture || texture->w <= 0 || texture->h <= 0) return;
  DrawTexture(renderer, texture, SDL_Rect{x, y, texture->w, texture->h});
}

void Rescan(AppState &app, bool show_message = false) {
  app.game_library.Scan();
  app.cover_cache.Clear(app.assets);
  RebuildShelfItems(app.shelf_runtime, app.game_library, app.layout.grid_cols);
  if (show_message) {
    ShowMessage(app, u8"游戏库已扫描：" +
                         std::to_string(app.game_library.Games().size()));
  }
}

void ClearRuntimeCache(AppState &app) {
  app.cover_cache.Clear(app.assets);
  const std::filesystem::path cache_root = app.config.root / "cache";
  std::error_code ec;
  for (std::filesystem::directory_iterator it(cache_root, std::filesystem::directory_options::skip_permission_denied, ec), end;
       it != end; it.increment(ec)) {
    if (ec) {
      ec.clear();
      continue;
    }
    const std::string name = ToLowerAscii(it->path().filename().u8string());
    if (name == "favorites.txt" || name == "history.txt") continue;
    std::filesystem::remove_all(it->path(), ec);
    ec.clear();
  }
  ShowMessage(app, LocalizedAppText(LanguageIndex(app), AppTextId::SystemClearCache));
}

void ClearHistory(AppState &app) {
  app.game_library.ClearHistory();
  if (app.shelf_runtime.category_index == 3) {
    RebuildShelfItems(app.shelf_runtime, app.game_library, app.layout.grid_cols);
  }
  ShowMessage(app, LocalizedAppText(LanguageIndex(app), AppTextId::SystemClearHistory));
}

void AdjustSystemSetting(AppState &app, int row, int delta) {
  switch (row) {
    case 0: {
      AdjustVolume(app, delta);
      break;
    }
    case 1: {
      if (app.system_settings_callbacks.adjust_brightness &&
          app.system_settings_callbacks.adjust_brightness(delta, SDL_GetTicks())) {
        SaveSetting(app, std::string(LocalizedAppText(LanguageIndex(app), AppTextId::SystemBrightness)) +
                             ": " + std::to_string(app.config.brightness_level));
      } else {
        ShowMessage(app, "System brightness unavailable");
      }
      break;
    }
    case 2:
      app.config.auto_sleep_enabled = delta < 0;
      SaveSetting(app, std::string(LocalizedAppText(LanguageIndex(app), AppTextId::SystemAutoSleep)) + ": " +
                           LocalizedAppText(LanguageIndex(app), app.config.auto_sleep_enabled
                                                                   ? AppTextId::SystemOn
                                                                   : AppTextId::SystemOff));
      break;
    case 3:
      app.config.auto_sleep_interval_index = std::clamp(app.config.auto_sleep_interval_index + delta, 0, 5);
      SaveSetting(app, std::string(LocalizedAppText(LanguageIndex(app), AppTextId::SystemSleepTimer)) + ": " +
                           LocalizedSleepIntervalLabel(LanguageIndex(app), app.config.auto_sleep_interval_index));
      break;
    case 4: {
      const int count = std::max(1, SystemLanguageCount());
      int index = LanguageIndex(app) + (delta > 0 ? 1 : -1);
      index = (index + count) % count;
      app.config.system_language = SystemLanguageConfigValue(index);
      SaveSetting(app, std::string(LocalizedAppText(index, AppTextId::SystemLanguage)) + ": " +
                           SystemLanguageDisplayLabel(index));
      break;
    }
    case 5:
      ClearRuntimeCache(app);
      break;
    case 6:
      ClearHistory(app);
      break;
  }
}

void HandleMenuInput(AppState &app, float dt) {
  SettingsPanelInputHandlers panel_handlers = MakeSettingsPanelInputHandlers(
      SettingsPanelInputComposition{
          app.input,
          app.calibration,
          app.version_update_state,
          app.contributor_avatar_state,
          app.game_settings_state,
          app.game_settings_callbacks,
          app.avatars.size(),
          !app.config.update_manifest_url.empty(),
          [&](int row, int delta) { AdjustSystemSetting(app, row, delta); },
          [&](const KeyCalibrationState &state) {
            return SaveKeyCalibrationMapping(app.config.root / "native_keymap.ini",
                                             DetectDeviceModelToken(), app.input, state);
          },
          [&]() { PlaySfx(app, SfxId::Select); },
          [&](int index) {
            if (index < 0 || index >= static_cast<int>(app.avatars.size())) return;
            app.config.selected_avatar = app.avatars[static_cast<size_t>(index)].filename;
            SaveSetting(app,
                        LocalizedAppText(LanguageIndex(app),
                                         AppTextId::SettingContributorAvatars));
          },
      });

  SettingsRuntimeInputActions actions;
  actions.menu_toggle_request = app.input.IsJustPressed(Button::Menu);
  actions.animations_enabled = true;
  actions.on_close = [&]() {
    app.menu_open = false;
    app.menu_state.closing = false;
  };
  actions.on_exit_app = [&]() { app.running = false; };
  actions.on_open_panel = [&](SettingId id, SettingsRuntimeState &) {
    if (id == SettingId::SystemControls && app.system_settings_callbacks.refresh_levels) {
      app.system_settings_callbacks.refresh_levels();
    }
    if (id == SettingId::GameSettings && app.game_settings_callbacks.refresh_state) {
      app.game_settings_callbacks.refresh_state(app.game_settings_state);
    }
    if (id == SettingId::KeyCalibration) app.input.ClearCalibrationSamples();
  };
  actions.panel_handlers = std::move(panel_handlers);

  app.menu_scene.Tick(app.menu_state, dt);
  SettingsRuntimeInputDeps deps{app.input, dt, app.menu_state, std::move(actions)};
  app.menu_scene.HandleInput(deps);
}

void HandleShelfInput(AppState &app, float dt) {
  bool title_needs_marquee = false;
#ifdef HAVE_SDL2_TTF
  if (const GameEntry *game = FocusedShelfGame(app.shelf_runtime, app.game_library)) {
    int width = 0;
    int height = 0;
    MeasureText(app.small_font, game->title, width, height);
    title_needs_marquee = width > std::max(8, app.layout.cover_w -
                                               app.layout.title_text_pad_x * 2);
  }
#endif
  TickShelfRuntime(app.shelf_runtime, dt, title_needs_marquee,
                   48.0f * app.layout.ui_scale);
  HandleShelfRuntimeInput(
      app.input, app.shelf_runtime, app.game_library, app.layout.grid_cols,
      ShelfRuntimeCallbacks{
          [&]() {
            app.menu_open = true;
            app.menu_scene.BeginOpen(app.menu_state, MenuSceneAnimationConfig{});
          },
          [&](const std::string &message) { ShowMessage(app, message); },
          [&](const GameEntry &game) {
            app.pending_game = game;
            app.launch_pending = true;
            app.running = false;
          },
      });
}

void HandleInput(AppState &app, float dt) {
  const int volume_delta = (app.input.Pressed(Button::VolUp) || app.input.Repeated(Button::VolUp)) ? 1
                           : (app.input.Pressed(Button::VolDown) || app.input.Repeated(Button::VolDown)) ? -1
                                                                                                        : 0;
  if (volume_delta != 0) {
    if (AdjustVolume(app, volume_delta)) {
      ScheduleVolumeReconcile(app.ui_state, SDL_GetTicks());
    }
  }
  const bool calibration_capturing = app.menu_open && app.menu_state.panel_active &&
      app.menu_scene.IsSelected(app.menu_state, SettingId::KeyCalibration) &&
      app.calibration.phase == KeyCalibrationPhase::Capturing;
  if (!calibration_capturing) {
    if (app.input.Pressed(Button::B)) PlaySfx(app, SfxId::Back);
    else if (app.input.Pressed(Button::A) || app.input.Pressed(Button::Menu)) PlaySfx(app, SfxId::Select);
    else if (app.input.Pressed(Button::Up) || app.input.Pressed(Button::Down) ||
             app.input.Pressed(Button::Left) || app.input.Pressed(Button::Right) ||
             app.input.Pressed(Button::L1) || app.input.Pressed(Button::R1)) PlaySfx(app, SfxId::Move);
  }
  if (app.menu_open) {
    HandleMenuInput(app, dt);
    return;
  }
  HandleShelfInput(app, dt);
}

void DrawStatusChrome(AppState &app) {
  DrawTextureNative(app.renderer, app.assets.Get("top_status_bar.png"), 0, 0);
  DrawTextureNative(app.renderer, app.assets.Get("bottom_hint_bar.png"), 0,
                    app.layout.screen_h - app.layout.bottom_bar_h);
}

void DrawSystemStatus(AppState &app) {
  StatusBarRenderDeps deps;
  deps.renderer = app.renderer;
  deps.status = &app.system_status.Snapshot();
  deps.input_profile = InputProfileName(app.input_profile);
  deps.screen_w = app.layout.screen_w;
  deps.top_bar_y = app.layout.top_bar_y;
  deps.top_bar_h = app.layout.top_bar_h;
  if (TextureHandle *avatar = SelectedAvatarTexture(app)) deps.selected_avatar_texture = avatar->texture;
  deps.scale_px = [&](int value) { return static_cast<int>(std::lround(value * app.layout.ui_scale)); };
  deps.draw_rect = [&](const SDL_Rect &rect, SDL_Color color, bool filled) {
    if (filled) Fill(app.renderer, rect, color);
    else Stroke(app.renderer, rect, color, 1);
  };
#ifdef HAVE_SDL2_TTF
  deps.get_text_texture = [&](const std::string &text, SDL_Color color) {
    return app.text_cache.Get(app.renderer, app.micro_font, text, color);
  };
#endif
  DrawStatusBarRuntime(deps);
}

void DrawVolumeOverlay(AppState &app) {
  VolumeOverlayRenderDeps deps;
  deps.renderer = app.renderer;
  deps.now = SDL_GetTicks();
  deps.display_until = app.ui_state.volume_display_until;
  deps.display_percent = app.ui_state.volume_display_percent;
  deps.top_bar_y = app.layout.top_bar_y;
  deps.top_bar_h = app.layout.top_bar_h;
  deps.scale_px = [&](int value) { return static_cast<int>(std::lround(value * app.layout.ui_scale)); };
#ifdef HAVE_SDL2_TTF
  deps.get_text_texture = [&](const std::string &text, SDL_Color color) {
    return app.text_cache.Get(app.renderer, app.micro_font, text, color);
  };
#endif
  DrawVolumeOverlayRuntime(deps);
}

void DrawShelf(AppState &app) {
  ShelfSceneRenderServices services;
  services.get_asset = [&](const std::string &name) { return app.assets.Get(name); };
  services.get_cover = [&](const GameEntry &game) {
    return ResolveGameCover(app.renderer, app.assets, app.cover_cache, game);
  };
#ifdef HAVE_SDL2_TTF
  services.draw_text = [&](const std::string &text, int x, int y, SDL_Color color) {
    DrawText(app, text, x, y, color, app.small_font);
  };
  services.measure_text = [&](const std::string &text, int &width, int &height) {
    MeasureText(app.small_font, text, width, height);
  };
  services.ellipsize = [&](const std::string &text, int width) {
    return Ellipsize(app.small_font, text, width);
  };
#endif
  app.shelf_scene.Draw(ShelfSceneRenderContext{app.renderer, app.layout,
                                               app.shelf_runtime, app.game_library,
                                               std::move(services)});
}

#ifdef HAVE_SDL2_TTF
TTF_Font *MenuPanelFont(AppState &app, MenuPanelTextStyle style) {
  switch (style) {
    case MenuPanelTextStyle::Title: return app.title_font;
    case MenuPanelTextStyle::Menu: return app.menu_font;
    case MenuPanelTextStyle::Small: return app.small_font;
    case MenuPanelTextStyle::Body: return app.font;
  }
  return app.font;
}
#endif

MenuPanelDrawServices MakeMenuPanelDrawServices(AppState &app) {
  MenuPanelDrawServices services;
  services.draw_rect = [&](const SDL_Rect &rect, SDL_Color color, bool filled, int thickness) {
    if (filled) Fill(app.renderer, rect, color);
    else Stroke(app.renderer, rect, color, thickness);
  };
#ifdef HAVE_SDL2_TTF
  services.draw_text = [&](const std::string &text, int x, int y, SDL_Color color,
                           MenuPanelTextStyle style) {
    DrawText(app, text, x, y, color, MenuPanelFont(app, style));
  };
  services.draw_text_centered = [&](const std::string &text, const SDL_Rect &rect,
                                    SDL_Color color, MenuPanelTextStyle style) {
    DrawTextCentered(app, text, rect, color, MenuPanelFont(app, style));
  };
  services.draw_text_right = [&](const std::string &text, int right, int y, SDL_Color color,
                                 MenuPanelTextStyle style) {
    DrawTextRight(app, text, right, y, color, MenuPanelFont(app, style));
  };
  services.measure_text = [&](const std::string &text, MenuPanelTextStyle style, int &w, int &h) {
    MeasureText(MenuPanelFont(app, style), text, w, h);
  };
  services.ellipsize = [&](const std::string &text, MenuPanelTextStyle style, int max_width) {
    return Ellipsize(MenuPanelFont(app, style), text, max_width);
  };
#endif
  return services;
}

void DrawMenu(AppState &app) {
  if (!app.menu_open) return;
  SettingsRuntimeRenderServices services;
  services.draw_rect = [&](int x, int y, int w, int h, SDL_Color color, bool filled) {
    const SDL_Rect rect{x, y, w, h};
    if (filled) Fill(app.renderer, rect, color);
    else Stroke(app.renderer, rect, color, 2);
  };
  services.get_texture = [&](const std::string &name) { return app.assets.Get(name); };
#ifdef HAVE_SDL2_TTF
  services.get_text_texture = [&](const std::string &text, SDL_Color color) {
    return app.text_cache.Get(app.renderer, app.menu_font, text, color);
  };
  services.get_title_text_texture = [&](const std::string &text, SDL_Color color) {
    return app.text_cache.Get(app.renderer, app.title_font, text, color);
  };
#endif
  services.draw_texture_fit = [&](TextureHandle *texture, const SDL_Rect &rect) {
    DrawTextureFit(app.renderer, texture, rect);
  };
  services.draw_chrome = [&]() { DrawStatusChrome(app); };

  SettingsPanelDrawHandlers panels = MakeSettingsPanelDrawHandlers(
      SettingsPanelDrawComposition{
          app.renderer,
          app.assets,
          app.layout,
          app.config,
          app.system_levels,
          app.menu_state,
          app.game_settings_state,
          app.calibration,
          app.avatars,
          app.contributor_avatar_state,
          app.version_update_state,
          MakeMenuPanelDrawServices(app),
      });

  SettingsRuntimeRenderDeps deps{
      app.renderer,
      app.config,
      app.input_profile,
      app.menu_state,
      MakeSettingsRuntimeLayout(app.layout),
      std::move(services),
      std::move(panels),
  };
  app.menu_scene.Draw(deps);
}

void DrawMessage(AppState &app) {
  if (app.message.empty() || SDL_GetTicks() > app.message_until) return;
  SDL_Rect box{440, 1210, 720, 72};
  Fill(app.renderer, box, Color(6, 10, 18, 220));
  Stroke(app.renderer, box, Color(100, 176, 245, 180), 2);
#ifdef HAVE_SDL2_TTF
  DrawTextCentered(app, app.message, box, Color(245, 248, 255), app.small_font);
#endif
}

void WriteRuntimeDiagnostics(const AppState &app) {
  std::ofstream out(app.config.root / "cache" / "ui_runtime.log", std::ios::trunc);
  if (!out) return;
  int window_w = 0;
  int window_h = 0;
  int output_w = 0;
  int output_h = 0;
  SDL_GetWindowSize(app.window, &window_w, &window_h);
  SDL_GetRendererOutputSize(app.renderer, &output_w, &output_h);
  out << "layout=" << app.layout.screen_w << "x" << app.layout.screen_h << "\n";
  out << "window=" << window_w << "x" << window_h << "\n";
  out << "renderer=" << output_w << "x" << output_h << "\n";
  out << "assets=" << app.assets.LoadedCount() << "\n";
  out << "text_cache=" << app.text_cache.Size() << "\n";
  out << "games=" << app.game_library.Games().size() << "\n";
  out << "visible=" << app.shelf_runtime.visible_games.size() << "\n";
  out << "root=" << app.config.root.u8string() << "\n";
  out << "profile=" << app.config.screen_profile << "\n";
  out << "input_profile=" << InputProfileName(app.input_profile) << "\n";
  out << "desktop_preview="
      << (app.platform_environment.capabilities.desktop_preview ? 1 : 0) << "\n";
  out << "packaging_verified="
      << (app.platform_environment.capabilities.packaging_verified ? 1 : 0) << "\n";
  out << "sdl_error=" << SDL_GetError() << "\n";
}

void CaptureRendererIfRequested(AppState &app) {
  if (app.capture_done) return;
  const char *capture_path = std::getenv("ROCGALGAME_CAPTURE_FRAME");
  if (!capture_path || !*capture_path) return;
  int w = 0;
  int h = 0;
  if (SDL_GetRendererOutputSize(app.renderer, &w, &h) != 0 || w <= 0 || h <= 0) return;
  SDL_Surface *surface = SDL_CreateRGBSurfaceWithFormat(0, w, h, 32, SDL_PIXELFORMAT_ARGB8888);
  if (!surface) return;
  if (SDL_RenderReadPixels(app.renderer, nullptr, surface->format->format, surface->pixels, surface->pitch) == 0) {
    SDL_SaveBMP(surface, capture_path);
  }
  SDL_FreeSurface(surface);
  app.capture_done = true;
  if (const char *exit_after_capture = std::getenv("ROCGALGAME_EXIT_AFTER_CAPTURE");
      exit_after_capture && std::string(exit_after_capture) == "1") {
    app.running = false;
  }
}

#ifdef HAVE_SDL2_TTF
TTF_Font *OpenFontAny(const std::vector<std::filesystem::path> &paths, int pt) {
  for (const auto &path : paths) {
    TTF_Font *font = TTF_OpenFont(path.u8string().c_str(), pt);
    if (font) return font;
  }
  return nullptr;
}
#endif

bool Init(AppState &app, AppBootstrapResult &bootstrap, int argc, char **argv, bool allow_autolaunch) {
  app.config_store.Load(argc > 0 ? argv[0] : nullptr);
  app.game_settings_state = MakeGameSettingsState(app.config);
  app.game_settings_callbacks = MakeGameSettingsCallbacks(
      app.config_store, []() { return SDL_GetTicks(); });
  app.lid_power_controller = LidPowerController(app.config.root / "pwr_new.sh");
  app.screen_profile = ResolveScreenProfile(app.config);
  const std::string device_model = DetectDeviceModelToken();
  app.input_profile = ResolveInputProfile(app.config.input_profile, device_model,
                                          app.screen_profile.profile_name);
  app.platform_environment = ResolveAppPlatformEnvironment(
      app.screen_profile, app.input_profile, device_model);
  app.layout = ResolveLayout(app.config, app.screen_profile);
  bootstrap = BootstrapSdlApp(app.layout);
  if (!bootstrap.ok) return false;
  app.window = bootstrap.window;
  app.renderer = bootstrap.renderer;
#ifdef HAVE_SDL2_TTF
  if (bootstrap.ttf_initialized) {
    const auto font_path = app.config.root / "fonts" / "ui_font.ttf";
    const auto cjk_font_path = app.config.root / "fonts" / "ui_font_02.ttf";
    const std::vector<std::filesystem::path> font_candidates = {cjk_font_path, font_path};
    app.title_font = OpenFontAny(font_candidates, 66);
    app.font = OpenFontAny(font_candidates, 32);
    app.menu_font = OpenFontAny(font_candidates, 44);
    app.small_font = OpenFontAny(font_candidates, 32);
    app.micro_font = OpenFontAny(font_candidates, 28);
    if (!app.title_font) app.title_font = app.font;
    if (!app.menu_font) app.menu_font = app.font;
    if (!app.small_font) app.small_font = app.font;
    if (!app.micro_font) app.micro_font = app.small_font;
  }
#endif
  app.input.Initialize((app.config.root / "native_keymap.ini").string(), app.input_profile);
  OpenAppInputDevices(app.input_devices);
  app.ui_state = InitializeAppUiState(app.config, app.volume_controller);
  if (app.config.system_volume_percent != app.ui_state.volume_display_percent) {
    app.config.system_volume_percent = app.ui_state.volume_display_percent;
    app.config_store.MarkDirty(SDL_GetTicks());
  }
  app.sfx_ready = app.sfx.Init(app.config.root);
  app.sfx.SetVolume(SfxVolumeFromSystemPercent(app.ui_state.volume_display_percent));
  app.system_settings_callbacks = MakeSystemSettingsCallbacks(
      app.volume_controller, app.system_controls, app.system_levels, app.config_store,
      app.ui_state, [&](int volume) { app.sfx.SetVolume(volume); },
      [&]() { PlaySfx(app, SfxId::Change); });
  app.power_lifecycle =
      std::make_unique<PowerLifecycleController>(app.input_profile, app.lid_power_controller);
  app.power_lifecycle->Initialize(SDL_GetTicks());
  app.assets.Load(app.renderer, app.config.root, app.screen_profile.profile_name);
  LoadAvatarEntries(app);
  if (const char *preview_volume = std::getenv("ROCGALGAME_PREVIEW_VOLUME");
      preview_volume && std::string(preview_volume) == "1") {
    ShowVolumeOverlay(app);
  }
  if (app.platform_environment.capabilities.has_brightness_control) {
    SynchronizeBrightnessAtStartup(app.system_controls, app.system_levels,
                                   app.config_store, SDL_GetTicks());
  }
  app.game_library.Configure(app.config);
  app.game_library.LoadPersistentState();
  Rescan(app);
  if (const char *nav_index = std::getenv("ROCGALGAME_NAV_INDEX"); nav_index && *nav_index) {
    try {
      app.shelf_runtime.category_index = std::clamp(std::stoi(nav_index), 0, 3);
      app.shelf_runtime.focus_index = 0;
      app.shelf_runtime.page = 0;
      RebuildShelfItems(app.shelf_runtime, app.game_library, app.layout.grid_cols);
    } catch (...) {}
  }
  if (const char *open_menu = std::getenv("ROCGALGAME_OPEN_MENU"); open_menu && std::string(open_menu) == "1") {
    app.menu_open = true;
    app.menu_state.anim.Snap(1.0f);
  }
  if (const char *menu_index = std::getenv("ROCGALGAME_MENU_INDEX"); menu_index && *menu_index) {
    try { app.menu_state.selected = std::clamp(std::stoi(menu_index), 0, static_cast<int>(kMenuItems.size()) - 1); } catch (...) {}
  }
  if (const char *panel = std::getenv("ROCGALGAME_OPEN_PANEL"); panel && std::string(panel) == "1") {
    app.menu_state.panel_active = true;
  }
  if (allow_autolaunch) {
    if (const char *autolaunch = std::getenv("ROCGALGAME_AUTOLAUNCH_FIRST");
        autolaunch && std::string(autolaunch) == "1") {
      if (const GameEntry *game = FocusedShelfGame(app.shelf_runtime, app.game_library)) {
        app.game_library.PushHistory(*game);
        app.pending_game = *game;
        app.launch_pending = true;
        app.running = false;
      }
    }
  }
  WriteRuntimeDiagnostics(app);
  return true;
}

void Shutdown(AppState &app) {
  if (app.config_store.IsDirty()) app.config_store.Save();
  if (app.power_lifecycle) app.power_lifecycle->EnsureScreenOn(SDL_GetTicks());
  CloseAppInputDevices(app.input_devices);
  app.sfx.Shutdown();
  app.text_cache.Clear();
#ifdef HAVE_SDL2_TTF
  if (app.title_font && app.title_font != app.font) TTF_CloseFont(app.title_font);
  if (app.font) TTF_CloseFont(app.font);
  if (app.menu_font && app.menu_font != app.font) TTF_CloseFont(app.menu_font);
  if (app.small_font && app.small_font != app.font) TTF_CloseFont(app.small_font);
  if (app.micro_font && app.micro_font != app.small_font && app.micro_font != app.font) TTF_CloseFont(app.micro_font);
#endif
  app.assets.Clear();
  app.renderer = nullptr;
  app.window = nullptr;
}
}  // namespace

int RunApp(int argc, char **argv) {
  OnsCoreAdapter ons_adapter;
  KrkrCoreAdapter krkr_adapter;
  GameCoreRegistry core_registry;
  core_registry.Register(&ons_adapter);
  core_registry.Register(&krkr_adapter);
  CoreProcessRunner core_process_runner;
  GameLaunchService game_launch_service(core_registry, core_process_runner);
  bool allow_autolaunch = true;
  int restore_nav = 0;
  int restore_focus = 0;
  int restore_page = 0;
  bool has_restore_state = false;
  std::string launch_message;
  while (true) {
    AppState app;
    AppBootstrapResult bootstrap;
    if (!Init(app, bootstrap, argc, argv, allow_autolaunch)) {
      Shutdown(app);
      const int exit_code = bootstrap.exit_code == 0 ? 1 : bootstrap.exit_code;
      ShutdownSdlApp(bootstrap);
      return exit_code;
    }
    AppContext context = MakeAppContext(app.window, app.renderer, app.screen_profile, app.layout,
                                        bootstrap.renderer_supports_target_textures);
    AppShell app_shell;
    app_shell.Initialize(context);
    allow_autolaunch = false;
    if (has_restore_state) {
      app.shelf_runtime.category_index = restore_nav;
      app.shelf_runtime.focus_index = restore_focus;
      app.shelf_runtime.page = restore_page;
      RebuildShelfItems(app.shelf_runtime, app.game_library, app.layout.grid_cols);
    }
    if (!launch_message.empty()) {
      ShowMessage(app, launch_message, 5000);
      launch_message.clear();
    }

    Uint32 last = SDL_GetTicks();
    while (app.running && app_shell.IsRunning()) {
      const AppFrameTiming frame = app_shell.BeginFrame(last);
      const Uint32 now = frame.now;
      const float dt = frame.dt;
      app.input.BeginFrame(dt);
      SDL_Event event;
      bool user_input = false;
      bool wake_consumed = false;
      while (SDL_PollEvent(&event)) {
        if (event.type == SDL_QUIT) {
          app.running = false;
          app_shell.RequestQuit();
        }
        if (IsUserInputEvent(event)) {
          user_input = true;
        }
        app.input.HandleEvent(event);
      }
      user_input = app.input.EndFrame() || user_input;
      const bool power_pressed = app.input.IsJustPressed(Button::Power);
      const bool non_power_activity = user_input && !power_pressed;
      const PowerLifecycleResult power_result = app.power_lifecycle->Update(
          now, app.config.auto_sleep_enabled &&
                   app.platform_environment.capabilities.has_screen_power_control,
          AutoSleepIntervalMs(app.config.auto_sleep_interval_index), power_pressed,
          non_power_activity);
      if (power_result.refresh_input_devices) {
        SDL_FlushEvents(SDL_FIRSTEVENT, SDL_LASTEVENT);
        RefreshAppInputDevices(app.input_devices);
        app.input.RefreshDevices();
        app.input.SuppressPowerUntilRelease();
      }
      wake_consumed = power_result.consumed_input;
      if (wake_consumed || app.power_lifecycle->IsScreenOff()) {
        app.input.ResetAll();
        last = SDL_GetTicks();
        SDL_Delay(10);
        continue;
      }
      HandleInput(app, dt);
      if (!app.running || !app_shell.IsRunning()) break;

      TickVolumeReconcile(now, app.volume_controller, app.config_store, app.ui_state,
                          [&](int volume) { app.sfx.SetVolume(volume); });

      constexpr Uint32 kConfigFlushDelayMs = 500;
      if (app.config_store.ShouldFlush(now, kConfigFlushDelayMs) && !app.config_store.Save()) {
        app.config_store.MarkDirty(now);
      }

      if (app.menu_open) app_shell.Scenes().EnterSettings();
      else app_shell.Scenes().EnterShelf();

      app.system_status.Poll(now);

      app_shell.BeginDraw();
      DrawShelf(app);
      DrawMenu(app);
      DrawSystemStatus(app);
      DrawVolumeOverlay(app);
      DrawMessage(app);
      CaptureRendererIfRequested(app);
      app_shell.Present();
    }

    const bool launch_pending = app.launch_pending;
    const GameEntry pending_game = app.pending_game;
    const AppConfig config = app.config;
    const int launch_language = SystemLanguageIndexFromConfigValue(config.system_language);
    restore_nav = app.shelf_runtime.category_index;
    restore_focus = app.shelf_runtime.focus_index;
    restore_page = app.shelf_runtime.page;
    if (launch_pending) app_shell.Scenes().EnterCoreTransition();
    Shutdown(app);
    ShutdownSdlApp(bootstrap);

    if (launch_pending) {
      has_restore_state = true;
      launch_message = DescribeLaunchResult(
          game_launch_service.Launch(config, pending_game), launch_language);
      continue;
    }
    return 0;
  }
}

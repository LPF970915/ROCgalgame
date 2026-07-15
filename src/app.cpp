#include "app.h"

#include "app_language.h"
#include "config.h"
#include "core_launcher.h"
#include "game_library.h"
#include "input.h"
#include "platform.h"
#include "system_controls.h"
#include "system_status.h"
#include "ui_assets.h"

#include <SDL.h>
#ifdef HAVE_SDL2_IMAGE
#include <SDL_image.h>
#endif
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
#include <sstream>
#include <string>
#include <vector>

namespace {
constexpr int kLaunchExitCode = 42;

enum class MenuItem {
  KeyGuide,
  KeyCalibration,
  SystemControls,
  GameSettings,
  Contributors,
  Contact,
  Exit,
};

constexpr std::array<MenuItem, 7> kMenuItems = {
    MenuItem::KeyGuide,
    MenuItem::KeyCalibration,
    MenuItem::SystemControls,
    MenuItem::GameSettings,
    MenuItem::Contributors,
    MenuItem::Contact,
    MenuItem::Exit,
};

constexpr std::array<const char *, 4> kNavLabels = {"ONS", "KRKR", "COLLECTIONS", "HISTORY"};

struct AvatarEntry {
  std::filesystem::path path;
  std::string filename;
  double contribution = 0.0;
  bool contribution_is_max = false;
};

struct AppState {
  AppConfig config;
  PlatformLayout layout;
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
  InputManager input;
#ifdef _WIN32
  SystemControlService system_controls{false};
#else
  SystemControlService system_controls{true};
#endif
  SystemControlLevels system_levels;
  SystemStatusMonitor system_status;
  std::vector<SDL_GameController *> controllers;
  std::vector<SDL_Joystick *> joysticks;
  std::vector<GameEntry> games;
  std::vector<int> visible_games;
  std::vector<std::string> favorites;
  std::vector<std::string> history;
  int focus = 0;
  int page = 0;
  int nav_selected = 0;
  bool running = true;
  bool launch_requested = false;
  bool launch_pending = false;
  GameEntry pending_game;
  bool capture_done = false;
  bool menu_open = false;
  bool menu_panel_active = false;
  int menu_focus = 0;
  int panel_focus = 0;
  int panel_button = 0;
  std::vector<AvatarEntry> avatars;
  int avatar_focus = 0;
  int avatar_scroll_row = 0;
  Uint32 last_user_input_tick = 0;
  bool screen_off = false;
  std::string message;
  Uint32 message_until = 0;
};

int ItemsPerView(const AppState &app) { return app.layout.cols * app.layout.rows; }
int FocusedCoverW(const AppState &app) { return static_cast<int>(app.layout.cover_w * app.layout.focus_scale + 0.5f); }
int FocusedCoverH(const AppState &app) { return static_cast<int>(app.layout.cover_h * app.layout.focus_scale + 0.5f); }
int LanguageIndex(const AppState &app) { return SystemLanguageIndexFromConfigValue(app.config.system_language); }

SDL_Color Color(Uint8 r, Uint8 g, Uint8 b, Uint8 a = 255) { return SDL_Color{r, g, b, a}; }

std::string NativePathString(const std::filesystem::path &path) {
#ifdef _WIN32
  return path.u8string();
#else
  return path.native();
#endif
}

std::string PathKey(const std::filesystem::path &path) {
  try {
    return ToLowerAscii(NativePathString(std::filesystem::weakly_canonical(path)));
  } catch (...) {
    try {
      return ToLowerAscii(NativePathString(path));
    } catch (...) {
      return {};
    }
  }
}

std::filesystem::path CachePath(const AppState &app, const char *name) {
  return app.config.root / "cache" / name;
}

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

#ifndef _WIN32
std::string ShellQuote(const std::string &text) {
  std::string out = "'";
  for (char ch : text) out += ch == '\'' ? "'\\''" : std::string(1, ch);
  out += "'";
  return out;
}
#endif

bool SetGkdDisplayPower(bool on) {
#ifdef _WIN32
  (void)on;
  return false;
#else
  const char *custom = std::getenv(on ? "ROCGALGAME_SCREEN_ON_COMMAND" : "ROCGALGAME_SCREEN_OFF_COMMAND");
  if (custom && *custom) return std::system(custom) == 0;
  const char *output_env = std::getenv("ROCGALGAME_GKD350H_OUTPUT");
  const std::string output = output_env && *output_env ? output_env : "DSI-1";
  const std::string command =
      "env XDG_RUNTIME_DIR=/var/run/0-runtime-dir WAYLAND_DISPLAY=wayland-1 "
      "SWAYSOCK=/var/run/0-runtime-dir/sway-ipc.0.sock swaymsg output " +
      ShellQuote(output) + " power " + (on ? "on" : "off") + " >/dev/null 2>&1";
  return std::system(command.c_str()) == 0;
#endif
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
    AvatarEntry avatar;
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
  std::sort(app.avatars.begin(), app.avatars.end(), [](const AvatarEntry &a, const AvatarEntry &b) {
    if (a.contribution_is_max != b.contribution_is_max) return !a.contribution_is_max;
    if (!a.contribution_is_max && a.contribution != b.contribution) return a.contribution > b.contribution;
    return a.filename < b.filename;
  });
  auto first_max = std::find_if(app.avatars.begin(), app.avatars.end(), [](const AvatarEntry &entry) {
    return entry.contribution_is_max;
  });
  if (first_max != app.avatars.end()) {
    std::vector<AvatarEntry> max_entries(first_max, app.avatars.end());
    app.avatars.erase(first_max, app.avatars.end());
    const size_t insert_at = std::min<size_t>(3, app.avatars.size());
    app.avatars.insert(app.avatars.begin() + static_cast<std::ptrdiff_t>(insert_at),
                       std::make_move_iterator(max_entries.begin()), std::make_move_iterator(max_entries.end()));
  }
  app.avatar_focus = 0;
  for (int i = 0; i < static_cast<int>(app.avatars.size()); ++i) {
    if ((!app.config.selected_avatar.empty() && app.avatars[static_cast<size_t>(i)].filename == app.config.selected_avatar) ||
        (app.config.selected_avatar.empty() && app.avatars[static_cast<size_t>(i)].filename.find("BloodROC") != std::string::npos)) {
      app.avatar_focus = i;
      break;
    }
  }
  if (!app.avatars.empty() && app.config.selected_avatar.empty()) {
    app.config.selected_avatar = app.avatars[static_cast<size_t>(app.avatar_focus)].filename;
  }
  app.avatar_scroll_row = std::max(0, app.avatar_focus / 3 - 1);
}

TextureHandle *SelectedAvatarTexture(AppState &app) {
  if (app.avatars.empty()) return nullptr;
  for (const AvatarEntry &entry : app.avatars) {
    if (entry.filename == app.config.selected_avatar) return app.assets.LoadExternal(app.renderer, entry.path);
  }
  return app.assets.LoadExternal(app.renderer, app.avatars.front().path);
}

std::vector<std::string> LoadPathList(const std::filesystem::path &path) {
  std::vector<std::string> out;
  std::ifstream in(path);
  std::string line;
  while (std::getline(in, line)) {
    line = Trim(line);
    if (!line.empty()) out.push_back(ToLowerAscii(line));
  }
  return out;
}

void SavePathList(const std::filesystem::path &path, const std::vector<std::string> &items) {
  std::error_code ec;
  std::filesystem::create_directories(path.parent_path(), ec);
  std::ofstream out(path, std::ios::trunc);
  if (!out) return;
  for (const std::string &item : items) out << item << "\n";
}

bool ListContains(const std::vector<std::string> &items, const std::string &key) {
  return std::find(items.begin(), items.end(), key) != items.end();
}

void ShowMessage(AppState &app, std::string text, Uint32 ms = 1800) {
  app.message = std::move(text);
  app.message_until = SDL_GetTicks() + ms;
}

std::string NextOption(const std::string &current, const std::vector<std::string> &options, int delta = 1) {
  if (options.empty()) return current;
  auto it = std::find(options.begin(), options.end(), current);
  int index = it == options.end() ? 0 : static_cast<int>(it - options.begin());
  index = (index + delta) % static_cast<int>(options.size());
  if (index < 0) index += static_cast<int>(options.size());
  return options[static_cast<size_t>(index)];
}

int NextIntOption(int current, const std::vector<int> &options, int delta = 1) {
  if (options.empty()) return current;
  auto it = std::find(options.begin(), options.end(), current);
  int index = it == options.end() ? 0 : static_cast<int>(it - options.begin());
  index = (index + delta) % static_cast<int>(options.size());
  if (index < 0) index += static_cast<int>(options.size());
  return options[static_cast<size_t>(index)];
}

float NextFloatOption(float current, const std::vector<float> &options, int delta = 1) {
  if (options.empty()) return current;
  int index = 0;
  float best = std::abs(current - options.front());
  for (int i = 1; i < static_cast<int>(options.size()); ++i) {
    const float d = std::abs(current - options[static_cast<size_t>(i)]);
    if (d < best) {
      best = d;
      index = i;
    }
  }
  index = (index + delta) % static_cast<int>(options.size());
  if (index < 0) index += static_cast<int>(options.size());
  return options[static_cast<size_t>(index)];
}

std::string AspectLabel(const std::string &value) {
  if (value == "stretch") return u8"拉伸全屏";
  if (value == "fit-width") return u8"等比适宽";
  if (value == "fill-height") return u8"等比适高";
  return u8"等比完整";
}

std::string FilterLabel(const std::string &value) {
  if (value == "crt-soft") return "CRT soft";
  if (value == "scanline") return u8"扫描线";
  if (value == "mask") return u8"像素遮罩";
  return u8"清晰";
}

std::string OnOffLabel(bool value) { return value ? u8"开" : u8"关"; }

std::string MouseAccelLabel(float value) {
  std::ostringstream oss;
  oss << std::fixed << std::setprecision(1) << value << "x";
  return oss.str();
}

void SaveSetting(AppState &app, const std::string &message) {
  ShowMessage(app, SaveAppConfig(app.config) ? message : u8"设置保存失败");
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
  SDL_Surface *surface = TTF_RenderUTF8_Blended(font, text.c_str(), color);
  if (!surface) return;
  SDL_Texture *texture = SDL_CreateTextureFromSurface(app.renderer, surface);
  SDL_Rect dst{x, y, surface->w, surface->h};
  SDL_FreeSurface(surface);
  if (!texture) return;
  SDL_RenderCopy(app.renderer, texture, nullptr, &dst);
  SDL_DestroyTexture(texture);
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

void DrawTextureCrop(SDL_Renderer *renderer, TextureHandle *texture, const SDL_Rect &dst) {
  if (!texture || !texture->texture || texture->w <= 0 || texture->h <= 0 || dst.w <= 0 || dst.h <= 0) return;
  const float src_aspect = static_cast<float>(texture->w) / static_cast<float>(texture->h);
  const float dst_aspect = static_cast<float>(dst.w) / static_cast<float>(dst.h);
  SDL_Rect src{0, 0, texture->w, texture->h};
  if (src_aspect > dst_aspect) {
    src.w = std::max(1, static_cast<int>(std::round(static_cast<float>(texture->h) * dst_aspect)));
    src.x = (texture->w - src.w) / 2;
  } else if (src_aspect < dst_aspect) {
    src.h = std::max(1, static_cast<int>(std::round(static_cast<float>(texture->w) / dst_aspect)));
    src.y = (texture->h - src.h) / 2;
  }
  SDL_RenderCopy(renderer, texture->texture, &src, &dst);
}

SDL_Rect OuterFrameRect(const AppState &app, const SDL_Rect &cover_rect) {
  const float sx = static_cast<float>(cover_rect.w) / static_cast<float>(app.layout.cover_w);
  const float sy = static_cast<float>(cover_rect.h) / static_cast<float>(app.layout.cover_h);
  const int outer_w = std::max(1, static_cast<int>(std::round(app.layout.card_frame_w * sx)));
  const int outer_h = std::max(1, static_cast<int>(std::round(app.layout.card_frame_h * sy)));
  const int cx = cover_rect.x + cover_rect.w / 2;
  const int cy = cover_rect.y + cover_rect.h / 2;
  return SDL_Rect{cx - outer_w / 2, cy - outer_h / 2, outer_w, outer_h};
}

bool GameMatchesNav(const AppState &app, const GameEntry &game) {
  const std::string key = PathKey(game.path);
  switch (app.nav_selected) {
    case 0: return game.core == CoreKind::Ons;
    case 1: return game.core == CoreKind::Krkr;
    case 2: return ListContains(app.favorites, key);
    case 3: return ListContains(app.history, key);
    default: return true;
  }
}

void ClampShelf(AppState &app) {
  const int count = static_cast<int>(app.visible_games.size());
  if (count <= 0) {
    app.focus = 0;
    app.page = 0;
    return;
  }
  app.focus = std::clamp(app.focus, 0, count - 1);
  const int max_row_page = (count - 1) / std::max(1, app.layout.cols);
  app.page = std::clamp(app.page, 0, max_row_page);
  const int col = std::clamp(app.focus % app.layout.cols, 0, app.layout.cols - 1);
  app.focus = std::min(count - 1, app.page * app.layout.cols + col);
}

void RebuildVisibleGames(AppState &app) {
  app.visible_games.clear();
  for (int i = 0; i < static_cast<int>(app.games.size()); ++i) {
    if (GameMatchesNav(app, app.games[static_cast<size_t>(i)])) app.visible_games.push_back(i);
  }
  ClampShelf(app);
}

const GameEntry *FocusedGame(const AppState &app) {
  if (app.visible_games.empty()) return nullptr;
  const int game_index = app.visible_games[static_cast<size_t>(std::clamp(app.focus, 0, static_cast<int>(app.visible_games.size()) - 1))];
  if (game_index < 0 || game_index >= static_cast<int>(app.games.size())) return nullptr;
  return &app.games[static_cast<size_t>(game_index)];
}

void Rescan(AppState &app, bool show_message = false) {
  app.games = ScanGameLibrary(app.config.root, app.config.games_root, app.config.covers_root, app.config.saves_root);
  app.assets.ClearExternal();
  RebuildVisibleGames(app);
  if (show_message) ShowMessage(app, u8"游戏库已扫描：" + std::to_string(app.games.size()));
}

void ChangeNav(AppState &app, int delta) {
  app.nav_selected = (app.nav_selected + delta) % static_cast<int>(kNavLabels.size());
  if (app.nav_selected < 0) app.nav_selected += static_cast<int>(kNavLabels.size());
  app.focus = 0;
  app.page = 0;
  RebuildVisibleGames(app);
}

void MoveFocus(AppState &app, int dx, int dy) {
  const int count = static_cast<int>(app.visible_games.size());
  if (count <= 0) return;
  const int cols = std::max(1, app.layout.cols);
  const int max_row_page = (count - 1) / cols;
  int col = app.focus % cols;
  if (dx < 0) {
    if (col > 0) --col;
    else if (app.page > 0) {
      --app.page;
      col = cols - 1;
    }
  } else if (dx > 0) {
    if (col < cols - 1) ++col;
    else if (app.page < max_row_page) {
      ++app.page;
      col = 0;
    }
  }
  if (dy < 0 && app.page > 0) --app.page;
  if (dy > 0 && app.page < max_row_page) ++app.page;
  app.page = std::clamp(app.page, 0, max_row_page);
  app.focus = std::min(count - 1, app.page * cols + col);
}

void AddFavorite(AppState &app, const GameEntry &game) {
  const std::string key = PathKey(game.path);
  if (key.empty() || ListContains(app.favorites, key)) return;
  app.favorites.push_back(key);
  SavePathList(CachePath(app, "favorites.txt"), app.favorites);
  ShowMessage(app, u8"已加入收藏");
  if (app.nav_selected == 2) RebuildVisibleGames(app);
}

void RemoveFavorite(AppState &app, const GameEntry &game) {
  const std::string key = PathKey(game.path);
  auto it = std::find(app.favorites.begin(), app.favorites.end(), key);
  if (it == app.favorites.end()) return;
  app.favorites.erase(it);
  SavePathList(CachePath(app, "favorites.txt"), app.favorites);
  ShowMessage(app, u8"已移出收藏");
  if (app.nav_selected == 2) RebuildVisibleGames(app);
}

void PushHistory(AppState &app, const GameEntry &game) {
  const std::string key = PathKey(game.path);
  if (key.empty()) return;
  app.history.erase(std::remove(app.history.begin(), app.history.end(), key), app.history.end());
  app.history.insert(app.history.begin(), key);
  if (app.history.size() > 64) app.history.resize(64);
  SavePathList(CachePath(app, "history.txt"), app.history);
}

std::string MenuLabel(MenuItem item, int language_index) {
  switch (item) {
    case MenuItem::KeyGuide: return LocalizedAppText(language_index, AppTextId::SettingKeyGuide);
    case MenuItem::KeyCalibration: return LocalizedAppText(language_index, AppTextId::SettingKeyCalibration);
    case MenuItem::SystemControls: return LocalizedAppText(language_index, AppTextId::SettingSystemControls);
    case MenuItem::GameSettings:
      if (language_index == 0) return u8"游戏设置";
      if (language_index == 1) return u8"遊戲設定";
      return "Game Settings";
    case MenuItem::Contributors: return LocalizedAppText(language_index, AppTextId::SettingContributorAvatars);
    case MenuItem::Contact: return LocalizedAppText(language_index, AppTextId::SettingContactMe);
    case MenuItem::Exit: return LocalizedAppText(language_index, AppTextId::SettingExitApp);
  }
  return {};
}

int PanelRowCount(const AppState &app, MenuItem item) {
  switch (item) {
    case MenuItem::SystemControls: return 7;
    case MenuItem::GameSettings: return 5;
    case MenuItem::Contributors: return static_cast<int>(app.avatars.size());
    default: return 0;
  }
}

void ClearRuntimeCache(AppState &app) {
  app.assets.ClearExternal();
  const std::filesystem::path cache_root = app.config.root / "cache";
  std::error_code ec;
  for (std::filesystem::directory_iterator it(cache_root, std::filesystem::directory_options::skip_permission_denied, ec), end;
       it != end; it.increment(ec)) {
    if (ec) {
      ec.clear();
      continue;
    }
    const std::string name = ToLowerAscii(it->path().filename().u8string());
    if (name == "favorites.txt" || name == "history.txt" || name == "launch_request.ini") continue;
    std::filesystem::remove_all(it->path(), ec);
    ec.clear();
  }
  ShowMessage(app, LocalizedAppText(LanguageIndex(app), AppTextId::SystemClearCache));
}

void ClearHistory(AppState &app) {
  app.history.clear();
  SavePathList(CachePath(app, "history.txt"), app.history);
  if (app.nav_selected == 3) RebuildVisibleGames(app);
  ShowMessage(app, LocalizedAppText(LanguageIndex(app), AppTextId::SystemClearHistory));
}

void AdjustSystemSetting(AppState &app, int row, int delta) {
  switch (row) {
    case 0: {
      if (app.system_controls.AdjustVolume(delta, app.system_levels) && app.system_levels.volume.available) {
        app.config.system_volume_percent = std::clamp(
            (app.system_levels.volume.level * 100) / std::max(1, app.system_levels.volume.max_level), 0, 100);
        SaveSetting(app, std::string(LocalizedAppText(LanguageIndex(app), AppTextId::SystemKeySound)) +
                             ": " + std::to_string(app.config.system_volume_percent) + "%");
      } else {
        ShowMessage(app, "System volume unavailable");
      }
      break;
    }
    case 1: {
      if (app.system_controls.AdjustBrightness(delta, app.system_levels) && app.system_levels.brightness.available) {
        app.config.brightness_level = std::clamp(app.system_levels.brightness.level, 0, 8);
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

void AdjustGameSetting(AppState &app, int row, int delta) {
  switch (row) {
    case 0:
      app.config.default_aspect = NextOption(app.config.default_aspect, {"stretch", "fit-width", "fill-height", "contain"}, delta);
      SaveSetting(app, std::string(u8"画面比例：") + AspectLabel(app.config.default_aspect));
      break;
    case 1:
      app.config.default_filter = NextOption(app.config.default_filter, {"clean", "scanline", "crt-soft", "mask"}, delta);
      SaveSetting(app, std::string(u8"画面滤镜：") + FilterLabel(app.config.default_filter));
      break;
    case 2:
      app.config.virtual_mouse = !app.config.virtual_mouse;
      SaveSetting(app, std::string(u8"虚拟鼠标：") + OnOffLabel(app.config.virtual_mouse));
      break;
    case 3:
      app.config.mouse_speed = NextIntOption(app.config.mouse_speed, {480, 720, 960, 1200}, delta);
      SaveSetting(app, u8"鼠标速度：" + std::to_string(app.config.mouse_speed));
      break;
    case 4:
      app.config.mouse_accel = NextFloatOption(app.config.mouse_accel, {1.0f, 1.3f, 1.6f, 2.0f}, delta);
      SaveSetting(app, std::string(u8"鼠标加速度：") + MouseAccelLabel(app.config.mouse_accel));
      break;
  }
}

void HandleMenuInput(AppState &app) {
  const MenuItem selected = kMenuItems[static_cast<size_t>(std::clamp(app.menu_focus, 0, static_cast<int>(kMenuItems.size()) - 1))];
  if (app.menu_panel_active) {
    const int rows = PanelRowCount(app, selected);
    if (app.input.Pressed(Button::B) || app.input.Pressed(Button::Menu)) {
      app.menu_panel_active = false;
      app.panel_focus = 0;
      app.panel_button = 0;
      return;
    }

    if (selected == MenuItem::Contributors && rows > 0) {
      int row = app.avatar_focus / 3;
      int col = app.avatar_focus % 3;
      if ((app.input.Pressed(Button::Left) || app.input.Repeated(Button::Left)) && col > 0) --col;
      else if ((app.input.Pressed(Button::Right) || app.input.Repeated(Button::Right)) &&
               app.avatar_focus + 1 < rows && col < 2) ++col;
      else if ((app.input.Pressed(Button::Up) || app.input.Repeated(Button::Up)) && row > 0) --row;
      else if ((app.input.Pressed(Button::Down) || app.input.Repeated(Button::Down)) && (row + 1) * 3 < rows) ++row;
      app.avatar_focus = std::min(rows - 1, row * 3 + col);
      const int focus_row = app.avatar_focus / 3;
      if (focus_row < app.avatar_scroll_row) app.avatar_scroll_row = focus_row;
      if (focus_row > app.avatar_scroll_row + 1) app.avatar_scroll_row = focus_row - 1;
      if (app.input.Pressed(Button::A)) {
        app.config.selected_avatar = app.avatars[static_cast<size_t>(app.avatar_focus)].filename;
        SaveSetting(app, LocalizedAppText(LanguageIndex(app), AppTextId::SettingContributorAvatars));
      }
      return;
    }

    if (rows > 0) {
      if (app.input.Pressed(Button::Up) || app.input.Repeated(Button::Up)) {
        app.panel_focus = (app.panel_focus + rows - 1) % rows;
        if (selected == MenuItem::SystemControls && app.panel_focus >= 5) app.panel_button = 1;
      }
      if (app.input.Pressed(Button::Down) || app.input.Repeated(Button::Down)) {
        app.panel_focus = (app.panel_focus + 1) % rows;
        if (selected == MenuItem::SystemControls && app.panel_focus >= 5) app.panel_button = 1;
      }
      if (selected == MenuItem::SystemControls) {
        const bool dual = app.panel_focus <= 4;
        if (dual && app.input.Pressed(Button::Left)) app.panel_button = 0;
        if (dual && app.input.Pressed(Button::Right)) app.panel_button = 1;
        int delta = 0;
        if (app.input.Pressed(Button::A)) delta = app.panel_button == 1 ? 1 : -1;
        else if (app.input.Repeated(Button::Left) && app.panel_button == 0) delta = -1;
        else if (app.input.Repeated(Button::Right) && app.panel_button == 1) delta = 1;
        if (delta != 0) AdjustSystemSetting(app, app.panel_focus, delta);
      } else {
        if (app.input.Pressed(Button::Left)) AdjustGameSetting(app, app.panel_focus, -1);
        if (app.input.Pressed(Button::Right) || app.input.Pressed(Button::A)) {
          AdjustGameSetting(app, app.panel_focus, 1);
        }
      }
    }
    return;
  }

  if (app.input.Pressed(Button::B) || app.input.Pressed(Button::Menu)) {
    app.menu_open = false;
    return;
  }
  if (app.input.Pressed(Button::Up) || app.input.Repeated(Button::Up)) app.menu_focus = (app.menu_focus + static_cast<int>(kMenuItems.size()) - 1) % static_cast<int>(kMenuItems.size());
  if (app.input.Pressed(Button::Down) || app.input.Repeated(Button::Down)) app.menu_focus = (app.menu_focus + 1) % static_cast<int>(kMenuItems.size());
  if (app.input.Pressed(Button::A) || app.input.Pressed(Button::Right)) {
    if (selected == MenuItem::Exit) {
      app.running = false;
    } else {
      app.menu_panel_active = true;
      app.panel_focus = 0;
      app.panel_button = 0;
      if (selected == MenuItem::SystemControls) app.system_controls.Refresh(app.system_levels);
    }
  }
}

void HandleShelfInput(AppState &app) {
  if (app.input.Pressed(Button::Menu)) {
    app.menu_open = true;
    app.menu_panel_active = false;
    return;
  }
  if (app.input.Pressed(Button::L1)) ChangeNav(app, -1);
  if (app.input.Pressed(Button::R1)) ChangeNav(app, 1);
  if (app.input.Pressed(Button::Left)) MoveFocus(app, -1, 0);
  if (app.input.Pressed(Button::Right)) MoveFocus(app, 1, 0);
  if (app.input.Pressed(Button::Up)) MoveFocus(app, 0, -1);
  if (app.input.Pressed(Button::Down)) MoveFocus(app, 0, 1);
  if (const GameEntry *game = FocusedGame(app)) {
    if (app.input.Pressed(Button::X)) AddFavorite(app, *game);
    if (app.input.Pressed(Button::Y)) RemoveFavorite(app, *game);
    if (app.input.Pressed(Button::A)) {
      if (game->core == CoreKind::Ons) {
        PushHistory(app, *game);
        app.pending_game = *game;
        app.launch_pending = true;
        app.running = false;
      } else if (WriteLaunchRequest(app.config, *game)) {
        PushHistory(app, *game);
        app.launch_requested = true;
        app.running = false;
      } else {
        ShowMessage(app, u8"启动请求写入失败");
      }
    }
  }
}

void HandleInput(AppState &app) {
  if (app.menu_open) {
    HandleMenuInput(app);
    return;
  }
  HandleShelfInput(app);
}

void DrawNavChrome(AppState &app) {
  DrawTextureNative(app.renderer, app.assets.Get("top_status_bar.png"), 0, 0);
  DrawTextureNative(app.renderer, app.assets.Get("bottom_hint_bar.png"), 0, app.layout.screen_h - app.layout.bottom_bar_h);
  DrawTextureNative(app.renderer, app.assets.Get("nav_l1_icon.png"), app.layout.nav_l1_x, app.layout.nav_l1_y);
  DrawTextureNative(app.renderer, app.assets.Get("nav_r1_icon.png"), app.layout.nav_r1_x, app.layout.nav_r1_y);

  TextureHandle *pill = app.assets.Get("nav_selected_pill.png");
  if (pill && pill->texture) {
    const int slot_center = app.layout.nav_start_x + app.nav_selected * app.layout.nav_slot_w + app.layout.nav_slot_w / 2;
    DrawTextureNative(app.renderer, pill, slot_center - pill->w / 2, app.layout.nav_y);
  } else {
    const int x = app.layout.nav_start_x + app.nav_selected * app.layout.nav_slot_w + 8;
    Fill(app.renderer, SDL_Rect{x, app.layout.nav_y, app.layout.nav_slot_w - 16, app.layout.nav_pill_h}, Color(105, 113, 130));
  }

#ifdef HAVE_SDL2_TTF
  const int nav_text_center_h = pill && pill->h > 0 ? pill->h : app.layout.nav_pill_h;
  for (int i = 0; i < static_cast<int>(kNavLabels.size()); ++i) {
    const int slot_x = app.layout.nav_start_x + i * app.layout.nav_slot_w;
    std::string label = Ellipsize(app.font, kNavLabels[static_cast<size_t>(i)], app.layout.nav_slot_w - 12);
    DrawTextCentered(app, label, SDL_Rect{slot_x, app.layout.nav_y, app.layout.nav_slot_w, nav_text_center_h},
                     Color(238, 242, 250), app.font);
  }
#endif
}

void DrawStatusChrome(AppState &app) {
  DrawTextureNative(app.renderer, app.assets.Get("top_status_bar.png"), 0, 0);
  DrawTextureNative(app.renderer, app.assets.Get("bottom_hint_bar.png"), 0,
                    app.layout.screen_h - app.layout.bottom_bar_h);
}

void DrawSystemStatus(AppState &app) {
  const SystemStatusSnapshot &status = app.system_status.Snapshot();
  const SDL_Color text{238, 242, 250, 255};
  const SDL_Color fill = status.charging ? Color(104, 214, 141) : text;
  const int center_y = app.layout.top_bar_y + app.layout.top_bar_h / 2;
#ifdef HAVE_SDL2_TTF
  if (!status.clock_text.empty()) {
    int w = 0, h = 0;
    MeasureText(app.micro_font, status.clock_text, w, h);
    DrawText(app, status.clock_text, 1468 - w, center_y - h / 2, text, app.micro_font);
  }
#endif

  const SDL_Rect avatar_rect{1512, 8, 64, 64};
  Fill(app.renderer, avatar_rect, Color(26, 32, 42, 220));
  Stroke(app.renderer, avatar_rect, Color(152, 185, 210, 235), 1);
  DrawTextureCrop(app.renderer, SelectedAvatarTexture(app), avatar_rect);

  if (!status.battery_available) return;
  const int icon_x = 1249;
  const int icon_y = 26;
  const int body_w = 47;
  const int body_h = 29;
  for (int inset = 0; inset < 3; ++inset) {
    Stroke(app.renderer, SDL_Rect{icon_x + inset, icon_y + inset, body_w - inset * 2, body_h - inset * 2}, text, 1);
  }
  Fill(app.renderer, SDL_Rect{icon_x + body_w, center_y - 5, 4, 10}, text);
  const int inner_w = body_w - 10;
  const int fill_w = std::clamp(inner_w * status.battery_percent / 100, 0, inner_w);
  if (fill_w > 0) Fill(app.renderer, SDL_Rect{icon_x + 5, icon_y + 5, fill_w, body_h - 10}, fill);
#ifdef HAVE_SDL2_TTF
  const std::string battery = std::to_string(status.battery_percent) + "%";
  int w = 0, h = 0;
  MeasureText(app.micro_font, battery, w, h);
  DrawText(app, battery, 1308, center_y - h / 2, text, app.micro_font);
#endif
}

void DrawTitleOverlay(AppState &app, const GameEntry &game, const SDL_Rect &cover_rect, bool focused) {
  DrawTexture(app.renderer, app.assets.Get("book_title_shadow.png"), cover_rect);
#ifdef HAVE_SDL2_TTF
  const int text_area_x = cover_rect.x + app.layout.title_text_pad_x;
  const int text_area_w = std::max(8, cover_rect.w - app.layout.title_text_pad_x * 2);
  const int text_area_h = std::max(12, app.layout.title_overlay_h - 2);
  const int text_area_y = cover_rect.y + cover_rect.h - text_area_h - app.layout.title_text_pad_bottom;
  const std::string title = focused ? game.title : Ellipsize(app.small_font, game.title, text_area_w);
  SDL_Rect clip{text_area_x, text_area_y, text_area_w, text_area_h};
  SDL_Rect old_clip{};
  const SDL_bool had_clip = SDL_RenderIsClipEnabled(app.renderer);
  if (had_clip == SDL_TRUE) SDL_RenderGetClipRect(app.renderer, &old_clip);
  SDL_RenderSetClipRect(app.renderer, &clip);
  DrawTextCentered(app, title, clip, focused ? Color(248, 250, 255) : Color(230, 236, 248, 244), app.small_font);
  SDL_RenderSetClipRect(app.renderer, had_clip == SDL_TRUE ? &old_clip : nullptr);
#endif
}

void DrawGameCard(AppState &app, const GameEntry &game, const SDL_Rect &cover_rect, bool focused) {
  const SDL_Rect outer = OuterFrameRect(app, cover_rect);
  DrawTexture(app.renderer, app.assets.Get("book_under_shadow.png"), outer);
  Fill(app.renderer, cover_rect, game.core == CoreKind::Krkr ? Color(50, 58, 78, 255) : Color(38, 48, 66, 255));
  TextureHandle *cover_tex = app.assets.LoadExternal(app.renderer, game.cover_path);
  if (!cover_tex) cover_tex = app.assets.Get(game.core == CoreKind::Krkr ? "book_cover_pdf.png" : "book_cover_txt.png");
  DrawTextureCrop(app.renderer, cover_tex, cover_rect);
  DrawTitleOverlay(app, game, cover_rect, focused);
  if (focused) DrawTexture(app.renderer, app.assets.Get("book_select.png"), outer);
}

void DrawShelf(AppState &app) {
  DrawTexture(app.renderer, app.assets.Get("background_main.png"), SDL_Rect{0, 0, app.layout.screen_w, app.layout.screen_h});

  const int begin = app.page * app.layout.cols;
  const int end = std::min(begin + ItemsPerView(app), static_cast<int>(app.visible_games.size()));
  int focused_local = -1;
  for (int i = begin; i < end; ++i) {
    if (i == app.focus) {
      focused_local = i;
      continue;
    }
    const int local = i - begin;
    const int col = local % app.layout.cols;
    const int row = local / app.layout.cols;
    const int x = app.layout.grid_x + col * app.layout.step_x;
    const int y = app.layout.grid_y + row * app.layout.step_y;
    const GameEntry &game = app.games[static_cast<size_t>(app.visible_games[static_cast<size_t>(i)])];
    DrawGameCard(app, game, SDL_Rect{x, y, app.layout.cover_w, app.layout.cover_h}, false);
  }
  if (focused_local >= begin && focused_local < end) {
    const int local = focused_local - begin;
    const int col = local % app.layout.cols;
    const int row = local / app.layout.cols;
    const int w = FocusedCoverW(app);
    const int h = FocusedCoverH(app);
    const int x = app.layout.grid_x + col * app.layout.step_x + (app.layout.cover_w - w) / 2;
    const int y = app.layout.grid_y + row * app.layout.step_y + (app.layout.cover_h - h) / 2;
    const GameEntry &game = app.games[static_cast<size_t>(app.visible_games[static_cast<size_t>(focused_local)])];
    DrawGameCard(app, game, SDL_Rect{x, y, w, h}, true);
  }

#ifdef HAVE_SDL2_TTF
  if (app.visible_games.empty()) {
    SDL_Rect empty{470, 575, 660, 128};
    Fill(app.renderer, empty, Color(24, 34, 46, 230));
    Stroke(app.renderer, empty, Color(82, 125, 158, 255), 2);
    DrawTextCentered(app, app.games.empty() ? u8"未找到游戏" : u8"当前分类为空", empty, Color(240, 246, 255), app.font);
  }
#endif

  DrawNavChrome(app);
}

void DrawSettingRow(AppState &app, const SDL_Rect &row, const std::string &label, const std::string &value,
                    bool selected) {
  Fill(app.renderer, row, selected ? Color(63, 119, 158, 255) : Color(57, 73, 96, 214));
  if (selected) {
    Fill(app.renderer, SDL_Rect{row.x, row.y, 8, row.h}, Color(139, 214, 255));
    Stroke(app.renderer, SDL_Rect{row.x - 2, row.y - 2, row.w + 4, row.h + 4}, Color(85, 152, 198, 208), 2);
  }
#ifdef HAVE_SDL2_TTF
  DrawText(app, label, row.x + 28, row.y + std::max(0, (row.h - 56) / 2), Color(230, 236, 248), app.menu_font);
  DrawTextRight(app, value, row.x + row.w - 28, row.y + std::max(0, (row.h - 56) / 2), Color(245, 248, 255), app.menu_font);
#endif
}

void DrawSystemPanel(AppState &app, const SDL_Rect &preview, int first_row_y, int row_pitch, int row_h) {
  const int language = LanguageIndex(app);
  const std::array<std::string, 7> labels = {{
      LocalizedAppText(language, AppTextId::SystemKeySound),
      LocalizedAppText(language, AppTextId::SystemBrightness),
      LocalizedAppText(language, AppTextId::SystemAutoSleep),
      LocalizedAppText(language, AppTextId::SystemSleepTimer),
      LocalizedAppText(language, AppTextId::SystemLanguage),
      LocalizedAppText(language, AppTextId::SystemClearCache),
      LocalizedAppText(language, AppTextId::SystemClearHistory),
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
#ifdef HAVE_SDL2_TTF
  for (const std::string &label : labels) {
    int w = 0, h = 0;
    MeasureText(app.menu_font, label, w, h);
    label_w = std::max(label_w, w);
  }
#endif
  const int control_left = base_x + label_w + 36;
  Fill(app.renderer, SDL_Rect{preview.x + 36, first_row_y - 36, preview.w - 72, 2}, Color(66, 95, 124));

  auto draw_label = [&](int row) {
#ifdef HAVE_SDL2_TTF
    DrawText(app, labels[static_cast<size_t>(row)], base_x,
             first_row_y + row * row_pitch + std::max(0, (row_h - 56) / 2), text, app.menu_font);
#endif
  };
  auto draw_button = [&](const SDL_Rect &rect, const std::string &caption, bool selected, bool lit = false) {
    Fill(app.renderer, rect, lit ? state_lit : (selected ? selected_fill : fill));
    Stroke(app.renderer, rect, selected ? selected_border : muted, selected ? 2 : 1);
#ifdef HAVE_SDL2_TTF
    DrawTextCentered(app, caption, rect, text, app.menu_font);
#endif
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
    draw_button(minus, "-", app.menu_panel_active && app.panel_focus == row && app.panel_button == 0);
    draw_button(plus, "+", app.menu_panel_active && app.panel_focus == row && app.panel_button == 1);
    const SystemControlValue &value = row == 0 ? app.system_levels.volume : app.system_levels.brightness;
    const SDL_Rect meter{meter_x, cy - button_h / 2, meter_w, button_h};
    Fill(app.renderer, meter, Color(62, 77, 92));
    const int max_level = std::max(1, value.max_level);
    const int level = value.available ? std::clamp(value.level, 0, max_level) : 0;
    const int fill_w = meter.w * level / max_level;
    if (fill_w > 0) Fill(app.renderer, SDL_Rect{meter.x, meter.y, fill_w, meter.h}, active);
    Stroke(app.renderer, meter, muted, 1);
  }

  draw_label(2);
  const int row2_y = first_row_y + row_pitch * 2 + (row_h - button_h) / 2;
  const int off_x = control_right - long_w;
  const int on_x = off_x - gap - long_w;
  draw_button(SDL_Rect{on_x, row2_y, long_w, button_h},
              LocalizedAppText(language, AppTextId::SystemOn),
              app.menu_panel_active && app.panel_focus == 2 && app.panel_button == 0,
              app.config.auto_sleep_enabled);
  draw_button(SDL_Rect{off_x, row2_y, long_w, button_h},
              LocalizedAppText(language, AppTextId::SystemOff),
              app.menu_panel_active && app.panel_focus == 2 && app.panel_button == 1,
              !app.config.auto_sleep_enabled);

  int selector_w = 340;
  const int selector_left = control_right - button_w - 20 - selector_w - 20 - button_w;
  auto draw_selector = [&](int row, const std::string &value) {
    draw_label(row);
    const int y = first_row_y + row_pitch * row + (row_h - button_h) / 2;
    draw_button(SDL_Rect{selector_left, y, button_w, button_h}, "<",
                app.menu_panel_active && app.panel_focus == row && app.panel_button == 0);
    draw_button(SDL_Rect{control_right - button_w, y, button_w, button_h}, ">",
                app.menu_panel_active && app.panel_focus == row && app.panel_button == 1);
#ifdef HAVE_SDL2_TTF
    DrawTextCentered(app, value,
                     SDL_Rect{selector_left + button_w + 20, y,
                              selector_w, button_h}, text, app.menu_font);
#endif
  };
  draw_selector(3, LocalizedSleepIntervalLabel(language, app.config.auto_sleep_interval_index));
  draw_selector(4, SystemLanguageDisplayLabel(language));

  for (int row = 5; row <= 6; ++row) {
    draw_label(row);
    const int y = first_row_y + row_pitch * row + (row_h - button_h) / 2;
    draw_button(SDL_Rect{control_right - long_w, y, long_w, button_h},
                LocalizedAppText(language, AppTextId::SystemClear),
                app.menu_panel_active && app.panel_focus == row);
  }
}

void DrawGameSettingsPanel(AppState &app, const SDL_Rect &preview) {
  const int x = preview.x + 88;
  int y = preview.y + 250;
  const int w = preview.w - 176;
  const int row_h = 82;
  const int pitch = 104;
  const std::array<std::pair<std::string, std::string>, 5> rows = {{
      {u8"画面比例", AspectLabel(app.config.default_aspect)},
      {u8"画面滤镜", FilterLabel(app.config.default_filter)},
      {u8"虚拟鼠标", OnOffLabel(app.config.virtual_mouse)},
      {u8"鼠标速度", std::to_string(app.config.mouse_speed)},
      {u8"鼠标加速度", MouseAccelLabel(app.config.mouse_accel)},
  }};
  for (int i = 0; i < static_cast<int>(rows.size()); ++i) {
    DrawSettingRow(app, SDL_Rect{x, y + i * pitch, w, row_h}, rows[static_cast<size_t>(i)].first,
                   rows[static_cast<size_t>(i)].second, app.menu_panel_active && app.panel_focus == i);
  }
}

std::string AvatarDisplayName(const std::string &filename) {
  size_t begin = filename.find('_');
  if (begin == std::string::npos) return filename;
  begin = filename.find('_', begin + 1);
  if (begin == std::string::npos) return filename;
  ++begin;
  size_t end = filename.find(u8"_贡献值", begin);
  if (end == std::string::npos) end = filename.find_last_of('.');
  return filename.substr(begin, end == std::string::npos ? std::string::npos : end - begin);
}

void DrawAvatarPanel(AppState &app, const SDL_Rect &preview, int first_row_y) {
  if (app.avatars.empty()) return;
  const int safe_left = preview.x + 48;
  const int safe_right = preview.x + preview.w - 48;
  const int safe_top = first_row_y - 20;
  const int safe_bottom = app.layout.bottom_bar_y - 40;
  const int gap_x = 28;
  const int tile_w = (safe_right - safe_left - gap_x * 2) / 3;
  const int row_pitch = 410;
  const int image_size = 280;
  SDL_Rect viewport{safe_left, safe_top, safe_right - safe_left, safe_bottom - safe_top};
  SDL_Rect old_clip{};
  const SDL_bool had_clip = SDL_RenderIsClipEnabled(app.renderer);
  if (had_clip == SDL_TRUE) SDL_RenderGetClipRect(app.renderer, &old_clip);
  SDL_RenderSetClipRect(app.renderer, &viewport);
  for (int i = 0; i < static_cast<int>(app.avatars.size()); ++i) {
    const int row = i / 3;
    const int col = i % 3;
    const int tile_x = safe_left + col * (tile_w + gap_x);
    const int tile_y = safe_top + 20 + (row - app.avatar_scroll_row) * row_pitch;
    if (tile_y + image_size < safe_top || tile_y > safe_bottom) continue;
    const bool focused = app.menu_panel_active && i == app.avatar_focus;
    const int draw_size = focused ? 302 : image_size;
    const int image_x = tile_x + (tile_w - draw_size) / 2;
    TextureHandle *avatar = app.assets.LoadExternal(app.renderer, app.avatars[static_cast<size_t>(i)].path);
    DrawTextureCrop(app.renderer, avatar, SDL_Rect{image_x, tile_y, draw_size, draw_size});
    int frame_no = -1;
    if (app.avatars[static_cast<size_t>(i)].contribution_is_max) frame_no = 0;
    else if (i < 3) frame_no = i + 1;
    if (frame_no >= 0) {
      TextureHandle *frame = app.assets.LoadExternal(
          app.renderer, app.config.root / "ui" / "common" / ("AvatarFrame_NO." + std::to_string(frame_no) + ".png"));
      DrawTexture(app.renderer, frame, SDL_Rect{image_x, tile_y, draw_size, draw_size});
    }
    if (focused) {
      Stroke(app.renderer, SDL_Rect{image_x - 6, tile_y - 6, draw_size + 12, draw_size + 12},
             Color(120, 205, 255), 4);
    }
#ifdef HAVE_SDL2_TTF
    const std::string label = Ellipsize(app.small_font, AvatarDisplayName(app.avatars[static_cast<size_t>(i)].filename), tile_w);
    DrawTextCentered(app, label, SDL_Rect{tile_x, tile_y + draw_size + 18, tile_w, 58},
                     Color(235, 241, 248), app.small_font);
#endif
  }
  SDL_RenderSetClipRect(app.renderer, had_clip == SDL_TRUE ? &old_clip : nullptr);
}

void DrawSimplePanel(AppState &app, const SDL_Rect &preview, MenuItem item) {
#ifdef HAVE_SDL2_TTF
  const int x = preview.x + 92;
  const int y = preview.y + 260;
  if (item == MenuItem::KeyCalibration) {
    DrawText(app, u8"校准面板预留", x, y, Color(230, 236, 248), app.font);
  } else if (item == MenuItem::Contact) {
    DrawText(app, "GitHub: LPF970915 / ROCreader", x, y, Color(230, 236, 248), app.menu_font);
  } else if (item == MenuItem::Exit) {
    DrawText(app, u8"按 A 退出", x, y, Color(230, 236, 248), app.font);
  }
#endif
}

void DrawSelectedPanel(AppState &app, const SDL_Rect &preview, int first_row_y, int row_pitch, int row_h) {
  const MenuItem selected = kMenuItems[static_cast<size_t>(std::clamp(app.menu_focus, 0, static_cast<int>(kMenuItems.size()) - 1))];
#ifdef HAVE_SDL2_TTF
  if (selected != MenuItem::SystemControls && selected != MenuItem::Contributors) {
    DrawText(app, MenuLabel(selected, LanguageIndex(app)), preview.x + 88, preview.y + 145,
             Color(240, 246, 255), app.title_font);
  }
#endif
  if (selected == MenuItem::SystemControls) DrawSystemPanel(app, preview, first_row_y, row_pitch, row_h);
  else if (selected == MenuItem::GameSettings) DrawGameSettingsPanel(app, preview);
  else if (selected == MenuItem::Contributors) DrawAvatarPanel(app, preview, first_row_y);
  else DrawSimplePanel(app, preview, selected);
}

void DrawMenu(AppState &app) {
  if (!app.menu_open) return;

  const int menu_width = app.layout.settings_sidebar_w;
  const int menu_y = app.layout.settings_y_offset;
  const int menu_h = app.layout.screen_h - menu_y;
  const SDL_Rect preview{menu_width, menu_y, app.layout.screen_w - menu_width, menu_h};

  Fill(app.renderer, SDL_Rect{0, 0, app.layout.screen_w, app.layout.screen_h}, Color(0, 0, 0, 100));
  if (TextureHandle *preview_tex = app.assets.Get("Menu_Default.png"); preview_tex && preview_tex->texture) {
    DrawTextureFit(app.renderer, preview_tex, preview);
  } else {
    Fill(app.renderer, preview, Color(14, 20, 30, 240));
  }

  Fill(app.renderer, SDL_Rect{0, menu_y, menu_width, menu_h}, Color(24, 34, 46));
  Fill(app.renderer, SDL_Rect{menu_width - 1, menu_y, 1, menu_h}, Color(82, 125, 158));

#ifdef HAVE_SDL2_TTF
  const std::string title = LocalizedAppText(LanguageIndex(app), AppTextId::MenuTitle);
  int title_w = 0;
  int title_h = 0;
  MeasureText(app.title_font, title, title_w, title_h);
  const int title_y = menu_y + static_cast<int>(10 * app.layout.ui_scale) + app.layout.settings_content_offset_y;
  DrawText(app, title, std::max(0, (menu_width - title_w) / 2), title_y, Color(240, 246, 255), app.title_font);
  const int divider_y = title_y + title_h + static_cast<int>(12 * app.layout.ui_scale);
#else
  const int divider_y = menu_y + 152;
#endif

  Fill(app.renderer, SDL_Rect{24, divider_y, menu_width - 48, 2}, Color(66, 95, 124));

  const int margin_x = 36;
  const int item_h = 84;
  const int item_pitch = 116;
  const int text_left = 72;
  int y = divider_y + 36;
  const int first_menu_item_y = y;
  const int visible_bottom = app.layout.bottom_bar_y;
  SDL_Rect old_clip{};
  const SDL_bool had_clip = SDL_RenderIsClipEnabled(app.renderer);
  if (had_clip == SDL_TRUE) SDL_RenderGetClipRect(app.renderer, &old_clip);
  SDL_Rect sidebar_clip{0, y - 18, menu_width, std::max(0, visible_bottom - y + 18)};
  SDL_RenderSetClipRect(app.renderer, &sidebar_clip);
  for (int i = 0; i < static_cast<int>(kMenuItems.size()); ++i) {
    const bool selected = i == app.menu_focus;
    SDL_Rect row{margin_x, y + i * item_pitch, menu_width - margin_x * 2, item_h};
    Fill(app.renderer, row, selected ? Color(63, 119, 158) : Color(57, 73, 96, 214));
    if (selected) {
      Fill(app.renderer, SDL_Rect{row.x, row.y, 8, row.h}, Color(139, 214, 255));
      Stroke(app.renderer, SDL_Rect{row.x - 2, row.y - 2, row.w + 4, row.h + 4}, Color(85, 152, 198, 208), 2);
    }
#ifdef HAVE_SDL2_TTF
    DrawText(app, MenuLabel(kMenuItems[static_cast<size_t>(i)], LanguageIndex(app)), text_left,
             row.y + std::max(0, (row.h - 56) / 2), Color(230, 236, 248), app.menu_font);
#endif
  }
  SDL_RenderSetClipRect(app.renderer, had_clip == SDL_TRUE ? &old_clip : nullptr);

  DrawSelectedPanel(app, preview, first_menu_item_y, item_pitch, item_h);
  DrawStatusChrome(app);
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
  out << "games=" << app.games.size() << "\n";
  out << "visible=" << app.visible_games.size() << "\n";
  out << "root=" << app.config.root.u8string() << "\n";
  out << "profile=" << app.config.screen_profile << "\n";
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
}

void CloseInputDevices(AppState &app) {
  for (SDL_GameController *controller : app.controllers) {
    if (controller) SDL_GameControllerClose(controller);
  }
  for (SDL_Joystick *joystick : app.joysticks) {
    if (joystick) SDL_JoystickClose(joystick);
  }
  app.controllers.clear();
  app.joysticks.clear();
}

void OpenInputDevices(AppState &app) {
  CloseInputDevices(app);
  SDL_JoystickEventState(SDL_ENABLE);
  for (int i = 0; i < SDL_NumJoysticks(); ++i) {
    if (SDL_IsGameController(i)) {
      if (SDL_GameController *controller = SDL_GameControllerOpen(i)) app.controllers.push_back(controller);
    } else {
      if (SDL_Joystick *joystick = SDL_JoystickOpen(i)) app.joysticks.push_back(joystick);
    }
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

bool Init(AppState &app, int argc, char **argv, bool allow_autolaunch) {
  app.config = LoadAppConfig(argc > 0 ? argv[0] : nullptr);
  app.layout = ResolveLayout(app.config);
  if (SDL_Init(SDL_INIT_VIDEO | SDL_INIT_AUDIO | SDL_INIT_GAMECONTROLLER | SDL_INIT_JOYSTICK | SDL_INIT_EVENTS) != 0) {
    std::cerr << "SDL_Init failed: " << SDL_GetError() << "\n";
    return false;
  }
#ifdef HAVE_SDL2_IMAGE
  IMG_Init(IMG_INIT_PNG | IMG_INIT_JPG | IMG_INIT_WEBP);
#endif
#ifdef HAVE_SDL2_TTF
  if (TTF_Init() == 0) {
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
  OpenInputDevices(app);

  uint32_t flags = SDL_WINDOW_SHOWN;
#if defined(__arm__) || defined(__aarch64__)
  flags |= SDL_WINDOW_FULLSCREEN;
#endif
  int window_w = app.layout.screen_w;
  int window_h = app.layout.screen_h;
  if (const char *value = std::getenv("ROCGALGAME_WINDOW_W"); value && *value) {
    try { window_w = std::max(320, std::stoi(value)); } catch (...) {}
  }
  if (const char *value = std::getenv("ROCGALGAME_WINDOW_H"); value && *value) {
    try { window_h = std::max(240, std::stoi(value)); } catch (...) {}
  }
  app.window = SDL_CreateWindow("ROCgalgame", SDL_WINDOWPOS_CENTERED, SDL_WINDOWPOS_CENTERED,
                                window_w, window_h, flags);
  if (!app.window) {
    std::cerr << "SDL_CreateWindow failed: " << SDL_GetError() << "\n";
    return false;
  }
  SDL_SetHint(SDL_HINT_RENDER_SCALE_QUALITY, "0");
  app.renderer = SDL_CreateRenderer(app.window, -1, SDL_RENDERER_ACCELERATED | SDL_RENDERER_PRESENTVSYNC);
  if (!app.renderer) app.renderer = SDL_CreateRenderer(app.window, -1, SDL_RENDERER_SOFTWARE);
  if (!app.renderer) {
    std::cerr << "SDL_CreateRenderer failed: " << SDL_GetError() << "\n";
    return false;
  }
  if (window_w != app.layout.screen_w || window_h != app.layout.screen_h) {
    SDL_RenderSetLogicalSize(app.renderer, app.layout.screen_w, app.layout.screen_h);
  }
  app.assets.Load(app.renderer, app.config.root, app.config.screen_profile);
  LoadAvatarEntries(app);
  if (!app.system_controls.ApplyVolumePercent(app.config.system_volume_percent, app.system_levels.volume)) {
    app.system_controls.RefreshVolumeOnly(app.system_levels.volume);
  }
  if (!app.system_controls.ApplyBrightnessLevel(app.config.brightness_level, app.system_levels.brightness)) {
    app.system_controls.Refresh(app.system_levels);
  }
  app.last_user_input_tick = SDL_GetTicks();
  app.favorites = LoadPathList(CachePath(app, "favorites.txt"));
  app.history = LoadPathList(CachePath(app, "history.txt"));
  Rescan(app);
  if (const char *open_menu = std::getenv("ROCGALGAME_OPEN_MENU"); open_menu && std::string(open_menu) == "1") {
    app.menu_open = true;
  }
  if (const char *menu_index = std::getenv("ROCGALGAME_MENU_INDEX"); menu_index && *menu_index) {
    try { app.menu_focus = std::clamp(std::stoi(menu_index), 0, static_cast<int>(kMenuItems.size()) - 1); } catch (...) {}
  }
  if (const char *panel = std::getenv("ROCGALGAME_OPEN_PANEL"); panel && std::string(panel) == "1") {
    app.menu_panel_active = true;
  }
  if (allow_autolaunch) {
    if (const char *autolaunch = std::getenv("ROCGALGAME_AUTOLAUNCH_FIRST");
        autolaunch && std::string(autolaunch) == "1") {
      if (const GameEntry *game = FocusedGame(app); game && game->core == CoreKind::Ons) {
        PushHistory(app, *game);
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
  if (app.screen_off) SetGkdDisplayPower(true);
  CloseInputDevices(app);
#ifdef HAVE_SDL2_TTF
  if (app.title_font && app.title_font != app.font) TTF_CloseFont(app.title_font);
  if (app.font) TTF_CloseFont(app.font);
  if (app.menu_font && app.menu_font != app.font) TTF_CloseFont(app.menu_font);
  if (app.small_font && app.small_font != app.font) TTF_CloseFont(app.small_font);
  if (app.micro_font && app.micro_font != app.small_font && app.micro_font != app.font) TTF_CloseFont(app.micro_font);
  TTF_Quit();
#endif
#ifdef HAVE_SDL2_IMAGE
  IMG_Quit();
#endif
  app.assets.Clear();
  if (app.renderer) SDL_DestroyRenderer(app.renderer);
  if (app.window) SDL_DestroyWindow(app.window);
  SDL_Quit();
}
}  // namespace

int RunApp(int argc, char **argv) {
  bool allow_autolaunch = true;
  while (true) {
    AppState app;
    if (!Init(app, argc, argv, allow_autolaunch)) {
      Shutdown(app);
      return 1;
    }
    allow_autolaunch = false;

    Uint32 last = SDL_GetTicks();
    while (app.running) {
      const Uint32 now = SDL_GetTicks();
      const float dt = std::min(0.05f, static_cast<float>(now - last) / 1000.0f);
      last = now;
      app.input.BeginFrame(dt);
      SDL_Event event;
      bool user_input = false;
      bool wake_consumed = false;
      while (SDL_PollEvent(&event)) {
        if (event.type == SDL_QUIT) app.running = false;
        if (IsUserInputEvent(event)) {
          user_input = true;
          if (app.screen_off) {
            if (SetGkdDisplayPower(true)) {
              app.screen_off = false;
              app.last_user_input_tick = now;
            }
            app.input.Reset();
            wake_consumed = true;
            continue;
          }
        }
        app.input.HandleEvent(event);
      }
      if (wake_consumed) continue;
      if (user_input) app.last_user_input_tick = now;
      HandleInput(app);
      if (!app.running) break;

      if (!app.screen_off && app.config.auto_sleep_enabled &&
          now - app.last_user_input_tick >= AutoSleepIntervalMs(app.config.auto_sleep_interval_index)) {
        app.last_user_input_tick = now;
        if (SetGkdDisplayPower(false)) {
          app.screen_off = true;
          app.input.Reset();
          continue;
        }
      }
      app.system_status.Poll(now);

      SDL_SetRenderDrawColor(app.renderer, 0, 0, 0, 255);
      SDL_RenderClear(app.renderer);
      DrawShelf(app);
      DrawMenu(app);
      DrawSystemStatus(app);
      DrawMessage(app);
      CaptureRendererIfRequested(app);
      SDL_RenderPresent(app.renderer);
    }

    const bool launch_pending = app.launch_pending;
    const bool launch_requested = app.launch_requested;
    const GameEntry pending_game = app.pending_game;
    const AppConfig config = app.config;
    Shutdown(app);

    if (launch_pending) {
      LaunchGameAndWait(config, pending_game);
      continue;
    }
    return launch_requested ? kLaunchExitCode : 0;
  }
}

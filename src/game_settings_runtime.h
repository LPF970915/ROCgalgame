#pragma once

#include "app_stores.h"
#include "config.h"
#include "game_settings_snapshot.h"
#include "input_manager.h"

#include <cstdint>
#include <functional>
#include <string>

struct SettingsRuntimeState;

enum class GameAspectMode {
  Stretch,
  FillHeight,
  Contain,
};

enum class GameFilterMode {
  Clean,
  Scanline,
  CrtSoft,
  Mask,
};

struct GameSettingsState {
  GameAspectMode aspect = GameAspectMode::Contain;
  GameFilterMode filter = GameFilterMode::Clean;
  bool virtual_mouse = true;
  int mouse_speed = 720;
  float mouse_acceleration = 1.6f;
};

struct GameSettingsCallbacks {
  std::function<void(GameSettingsState &)> refresh_state;
  std::function<bool(GameAspectMode, GameSettingsState &)> set_aspect;
  std::function<bool(GameFilterMode, GameSettingsState &)> set_filter;
  std::function<bool(bool, GameSettingsState &)> set_virtual_mouse;
  std::function<bool(int, GameSettingsState &)> set_mouse_speed;
  std::function<bool(float, GameSettingsState &)> set_mouse_acceleration;
};

GameAspectMode ParseGameAspectMode(const std::string &value);
const char *GameAspectConfigValue(GameAspectMode value);
GameFilterMode ParseGameFilterMode(const std::string &value);
const char *GameFilterConfigValue(GameFilterMode value);

GameSettingsState MakeGameSettingsState(const AppConfig &config);
GameSettingsCallbacks MakeGameSettingsCallbacks(
    ConfigStore &config_store, const std::function<uint32_t()> &now_provider);

std::string LocalizedGameSettingsLabel(int language_index, int row);
std::string LocalizedGameAspectLabel(int language_index, GameAspectMode value);
std::string LocalizedGameFilterLabel(int language_index, GameFilterMode value);
std::string GameMouseAccelerationLabel(float value);

bool HandleGameSettingsInput(const InputManager &input,
                             SettingsRuntimeState &menu_state,
                             GameSettingsState &state,
                             const GameSettingsCallbacks &callbacks);

#pragma once

#include "config.h"

#include <string>

struct GameSettingsSnapshot {
  std::string aspect;
  std::string filter;
  bool virtual_mouse = true;
  int mouse_speed = 1080;
  float mouse_acceleration = 1.0f;
};

inline GameSettingsSnapshot CaptureGameSettings(const AppConfig &config) {
  return GameSettingsSnapshot{config.default_aspect, config.default_filter,
                              config.virtual_mouse, config.mouse_speed,
                              config.mouse_accel};
}

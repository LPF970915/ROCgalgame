#pragma once

#include "screen_profile.h"
#include "input_manager.h"

#include <SDL.h>

#include <string>

struct AppRenderEnvironment {
  SDL_Window *window = nullptr;
  SDL_Renderer *renderer = nullptr;
  ScreenProfile screen_profile{};
};

struct PlatformCapabilities {
  bool desktop_preview = false;
  bool has_system_volume = false;
  bool has_brightness_control = false;
  bool has_screen_power_control = false;
  bool has_evdev_input = false;
  bool can_launch_external_cores = true;
  bool packaging_verified = false;
};

struct AppPlatformEnvironment {
  PlatformCapabilities capabilities;
  std::string device_model_token;
};

AppPlatformEnvironment ResolveAppPlatformEnvironment(
    const ScreenProfile &screen_profile, InputProfile input_profile,
    const std::string &device_model_token);

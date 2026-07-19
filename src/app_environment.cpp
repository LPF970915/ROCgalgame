#include "app_environment.h"

AppPlatformEnvironment ResolveAppPlatformEnvironment(
    const ScreenProfile &, InputProfile input_profile,
    const std::string &device_model_token) {
  AppPlatformEnvironment environment;
  environment.device_model_token = device_model_token;
  const bool desktop = input_profile == InputProfile::DesktopDefault;
  environment.capabilities.desktop_preview = desktop;
  environment.capabilities.can_launch_external_cores = true;
  if (desktop) return environment;

  environment.capabilities.has_system_volume = true;
  environment.capabilities.has_brightness_control = true;
  environment.capabilities.has_screen_power_control = true;
  environment.capabilities.has_evdev_input = true;
  environment.capabilities.packaging_verified =
      input_profile == InputProfile::GKD350HUltra;
  return environment;
}

#include "app_composition.h"
#include "app_environment.h"
#include "app_layout.h"
#include "scene_manager.h"
#include "screen_profile.h"

#include <cassert>
#include <iostream>

int main() {
  AppConfig config;
  config.screen_profile = "1600x1440";
  config.screen_w = 1600;
  config.screen_h = 1440;

  const ScreenProfile gkd_profile = ResolveScreenProfile(config);
  assert(gkd_profile.profile_name == "1600x1440");
  assert(gkd_profile.screen_w == 1600);
  assert(gkd_profile.screen_h == 1440);

  const LayoutMetrics gkd_layout = ResolveLayout(config, gkd_profile);
  assert(gkd_layout.screen_w == 1600);
  assert(gkd_layout.screen_h == 1440);
  assert(gkd_layout.grid_cols == 4);
  assert(gkd_layout.visible_rows == 3);
  assert(gkd_layout.grid_start_x == 74);
  assert(gkd_layout.grid_start_y == 222);
  assert(GridStepX(gkd_layout) == 386);
  assert(GridStepY(gkd_layout) == 541);
  assert(FocusedCoverWidth(gkd_layout) == 328);
  assert(FocusedCoverHeight(gkd_layout) == 492);

  config.screen_profile = "720x480";
  config.screen_w = 720;
  config.screen_h = 480;
  const ScreenProfile desktop_profile = ResolveScreenProfile(config);
  const LayoutMetrics desktop_layout = ResolveLayout(config, desktop_profile);
  assert(desktop_layout.screen_w == 720);
  assert(desktop_layout.screen_h == 480);
  assert(desktop_layout.grid_cols == 4);
  assert(desktop_layout.visible_rows == 2);

  const AppPlatformEnvironment desktop_environment = ResolveAppPlatformEnvironment(
      desktop_profile, InputProfile::DesktopDefault, "desktop");
  assert(desktop_environment.capabilities.desktop_preview);
  assert(!desktop_environment.capabilities.has_system_volume);
  assert(!desktop_environment.capabilities.has_brightness_control);
  assert(!desktop_environment.capabilities.has_screen_power_control);

  const AppPlatformEnvironment gkd_environment = ResolveAppPlatformEnvironment(
      gkd_profile, InputProfile::GKD350HUltra, "gkd350h-ultra");
  assert(!gkd_environment.capabilities.desktop_preview);
  assert(gkd_environment.capabilities.has_evdev_input);
  assert(gkd_environment.capabilities.packaging_verified);

  for (InputProfile profile : {InputProfile::H700Default, InputProfile::H70034xxSp,
                               InputProfile::H70035xxH, InputProfile::TrimuiBrick,
                               InputProfile::GKD350HUltra, InputProfile::RGDS}) {
    const AppPlatformEnvironment environment = ResolveAppPlatformEnvironment(
        gkd_profile, profile, InputProfileName(profile));
    assert(environment.capabilities.has_system_volume);
    assert(environment.capabilities.has_screen_power_control);
  }

  AppContext context = MakeAppContext(nullptr, nullptr, gkd_profile, gkd_layout, false);
  assert(context.ScreenWidth() == 1600);
  assert(context.ScreenHeight() == 1440);
  assert(context.layout == &gkd_layout);

  SceneManager scenes;
  assert(scenes.Is(AppScene::Shelf));
  scenes.EnterSettings();
  assert(scenes.Is(AppScene::Settings));
  scenes.EnterCoreTransition();
  assert(scenes.Is(AppScene::CoreTransition));
  scenes.EnterShelf();
  assert(scenes.Is(AppScene::Shelf));

  std::cout << "app foundation tests passed\n";
  return 0;
}

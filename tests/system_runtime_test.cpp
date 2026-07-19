#include "app_runtime.h"
#include "power_lifecycle.h"
#include "system_settings_runtime.h"

#include <cassert>
#include <filesystem>
#include <fstream>
#include <iostream>
#include <string>
#include <vector>

namespace {
ConfigStore MakeConfigStore(const std::filesystem::path &path) {
  std::ofstream out(path, std::ios::trunc);
  out << "system_volume_percent=50\nbrightness_level=4\n";
  out.close();
  ConfigStore store;
  store.LoadFromPath(path);
  return store;
}

void TestVolumeUsesOneAuthoritativeResultAndSfxOrder() {
  const auto path = std::filesystem::path("build") / "system_runtime_volume.ini";
  ConfigStore config = MakeConfigStore(path);
  SystemControlLevels levels;
  levels.volume = {true, 10, 20};
  VolumeController volume(
      levels,
      [](int delta, SystemControlLevels &current) {
        current.volume.level += delta;
        return true;
      },
      [](SystemControlValue &) { return true; });
  AppUiState ui{50, 0};
  std::vector<std::string> calls;
  int applied_sfx = -1;

  const auto result = ApplyVolumeAdjustment(
      1, 1000, volume, config, ui,
      [&](int value) {
        applied_sfx = value;
        calls.push_back("apply");
      },
      [&]() { calls.push_back("play"); });

  assert(result.changed);
  assert(result.percent == 55);
  assert(config.Get().system_volume_percent == 55);
  assert(ui.volume_display_percent == 55);
  assert(ui.volume_display_until == 2500);
  assert(applied_sfx == SfxVolumeFromSystemPercent(55));
  assert((calls == std::vector<std::string>{"apply", "play"}));
}

void TestVolumeFailureRefreshesButDoesNotFakeSuccess() {
  const auto path = std::filesystem::path("build") / "system_runtime_volume_failure.ini";
  ConfigStore config = MakeConfigStore(path);
  SystemControlLevels levels;
  levels.volume = {true, 10, 20};
  int refreshes = 0;
  VolumeController volume(
      levels, [](int, SystemControlLevels &) { return false; },
      [&](SystemControlValue &value) {
        ++refreshes;
        value = {true, 8, 20};
        return true;
      });
  AppUiState ui{50, 0};
  int side_effects = 0;
  const auto result = ApplyVolumeAdjustment(
      -1, 1000, volume, config, ui, [&](int) { ++side_effects; },
      [&]() { ++side_effects; });
  assert(!result.changed);
  assert(refreshes == 1);
  assert(config.Get().system_volume_percent == 50);
  assert(ui.volume_display_percent == 50);
  assert(side_effects == 0);
}

void TestVolumeReconcileIsIdempotentAndSilent() {
  const auto path = std::filesystem::path("build") / "system_runtime_reconcile.ini";
  ConfigStore config = MakeConfigStore(path);
  SystemControlLevels levels;
  levels.volume = {true, 11, 20};
  int apply_calls = 0;
  VolumeController volume(
      levels, [](int, SystemControlLevels &) { return true; },
      [](SystemControlValue &) { return true; },
      [&](int percent, SystemControlValue &value) {
        ++apply_calls;
        value = {true, percent / 5, 20};
        return true;
      });
  AppUiState ui;
  ui.volume_display_percent = 55;
  ScheduleVolumeReconcile(ui, 1000);
  int sfx_apply_calls = 0;
  assert(!TickVolumeReconcile(1249, volume, config, ui,
                              [&](int) { ++sfx_apply_calls; }));
  assert(TickVolumeReconcile(1250, volume, config, ui,
                             [&](int) { ++sfx_apply_calls; }));
  assert(apply_calls == 1);
  assert(sfx_apply_calls == 1);
  assert(config.Get().system_volume_percent == 55);
  assert(!ui.volume_reconcile_pending);
}

void TestBrightnessPersistsOnlyAppliedHardwareState() {
  const auto path = std::filesystem::path("build") / "system_runtime_brightness.ini";
  ConfigStore config = MakeConfigStore(path);
  SystemControlLevels levels;
  levels.brightness = {true, 4, 8};
  const bool changed = ApplyBrightnessAdjustment(
      1, 2000, levels, config,
      [](int delta, SystemControlLevels &current) {
        current.brightness.level += delta;
        return true;
      }, {});
  assert(changed);
  assert(config.Get().brightness_level == 5);

  int refreshes = 0;
  const bool failed = ApplyBrightnessAdjustment(
      1, 2100, levels, config, [](int, SystemControlLevels &) { return false; },
      [&](SystemControlLevels &current) {
        ++refreshes;
        current.brightness = {false, 0, 8};
      });
  assert(!failed);
  assert(refreshes == 1);
  assert(!levels.brightness.available);
  assert(config.Get().brightness_level == 5);
}

void TestSystemSettingsOwnsAudioCallbacks() {
  const auto path = std::filesystem::path("build") / "system_runtime_callbacks.ini";
  ConfigStore config = MakeConfigStore(path);
  SystemControlLevels levels;
  levels.volume = {true, 10, 20};
  VolumeController volume(
      levels,
      [](int delta, SystemControlLevels &current) {
        current.volume.level += delta;
        return true;
      },
      [](SystemControlValue &) { return true; });
  SystemControlService controls(false);
  AppUiState ui{50, 0};
  int applied = 0;
  int played = 0;
  SystemSettingsCallbacks callbacks = MakeSystemSettingsCallbacks(
      volume, controls, levels, config, ui,
      [&](int) { ++applied; }, [&]() { ++played; });
  assert(callbacks.adjust_volume(1, 3000));
  assert(applied == 1);
  assert(played == 1);
}

void TestPowerLifecycleSeparatesAutoAndManualWake() {
  int off_calls = 0;
  int on_calls = 0;
  PowerLifecycleController power(
      InputProfile::GKD350HUltra, [&]() { ++off_calls; return true; },
      [&]() { ++on_calls; return true; });
  power.Initialize(100);

  auto result = power.Update(1100, true, 1000, false, false);
  assert(result.display_turned_off);
  assert(power.Mode() == ScreenOffMode::Auto);
  result = power.Update(1200, true, 1000, false, true);
  assert(result.display_turned_on && result.refresh_input_devices);
  assert(power.Mode() == ScreenOffMode::Awake);

  result = power.Update(2100, true, 1000, true, false);
  assert(result.display_turned_off);
  assert(power.Mode() == ScreenOffMode::Manual);
  result = power.Update(2200, true, 1000, false, true);
  assert(!result.display_turned_on);
  assert(power.Mode() == ScreenOffMode::Manual);

  result = power.Update(2300, true, 1000, true, false);
  assert(result.consumed_input && !result.display_turned_on);
  result = power.Update(2949, true, 1000, false, false);
  assert(!result.display_turned_on);
  result = power.Update(2950, true, 1000, false, false);
  assert(result.display_turned_on && result.refresh_input_devices);
  assert(power.Mode() == ScreenOffMode::Awake);
  assert(off_calls == 2);
  assert(on_calls == 2);
}
}  // namespace

int main() {
  std::filesystem::create_directories("build");
  TestVolumeUsesOneAuthoritativeResultAndSfxOrder();
  TestVolumeFailureRefreshesButDoesNotFakeSuccess();
  TestVolumeReconcileIsIdempotentAndSilent();
  TestBrightnessPersistsOnlyAppliedHardwareState();
  TestSystemSettingsOwnsAudioCallbacks();
  TestPowerLifecycleSeparatesAutoAndManualWake();
  std::cout << "system runtime tests passed\n";
  return 0;
}

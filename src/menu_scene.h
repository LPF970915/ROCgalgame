#pragma once

#include "app_layout.h"
#include "settings_runtime.h"

struct MenuSceneAnimationConfig {
  bool animations_enabled = true;
  float close_duration_sec = 0.16f;
  float open_duration_sec = 0.20f;
  float toggle_guard_sec = 0.12f;
};

class MenuScene {
public:
  void Tick(SettingsRuntimeState &state, float dt) const;
  bool IsSelected(const SettingsRuntimeState &state, SettingId id) const;
  bool IsAnimating(const SettingsRuntimeState &state) const;
  bool CanCloseWithToggle(const SettingsRuntimeState &state) const;
  void BeginClose(SettingsRuntimeState &state, const MenuSceneAnimationConfig &config) const;
  void BeginOpen(SettingsRuntimeState &state, const MenuSceneAnimationConfig &config) const;
  void SnapClosed(SettingsRuntimeState &state) const;
  void HandleInput(SettingsRuntimeInputDeps &deps) const;
  void Draw(SettingsRuntimeRenderDeps &deps) const;
};

SettingsRuntimeLayout MakeSettingsRuntimeLayout(const LayoutMetrics &layout);

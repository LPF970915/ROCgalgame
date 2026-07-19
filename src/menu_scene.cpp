#include "menu_scene.h"

#include <algorithm>

void MenuScene::Tick(SettingsRuntimeState &state, float dt) const {
  state.toggle_guard = std::max(0.0f, state.toggle_guard - dt);
}

bool MenuScene::IsSelected(const SettingsRuntimeState &state, SettingId id) const {
  if (state.items.empty()) return false;
  const int selected = std::clamp(state.selected, 0, static_cast<int>(state.items.size()) - 1);
  return state.items[static_cast<size_t>(selected)] == id;
}

bool MenuScene::IsAnimating(const SettingsRuntimeState &state) const {
  return state.anim.IsAnimating();
}

bool MenuScene::CanCloseWithToggle(const SettingsRuntimeState &state) const {
  return state.close_armed && state.toggle_guard <= 0.0f && !state.closing;
}

void MenuScene::BeginClose(SettingsRuntimeState &state,
                           const MenuSceneAnimationConfig &config) const {
  if (config.animations_enabled) {
    state.anim.AnimateTo(0.0f, config.close_duration_sec, animation::Ease::InOutCubic);
  } else {
    state.anim.Snap(0.0f);
  }
  state.closing = true;
}

void MenuScene::BeginOpen(SettingsRuntimeState &state,
                          const MenuSceneAnimationConfig &config) const {
  state.anim.Snap(0.0f);
  if (config.animations_enabled) {
    state.anim.AnimateTo(1.0f, config.open_duration_sec, animation::Ease::OutCubic);
  } else {
    state.anim.Snap(1.0f);
  }
  state.toggle_guard = config.toggle_guard_sec;
  state.close_armed = false;
  state.closing = false;
  state.panel_active = false;
}

void MenuScene::SnapClosed(SettingsRuntimeState &state) const { state.anim.Snap(0.0f); }

void MenuScene::HandleInput(SettingsRuntimeInputDeps &deps) const { HandleSettingsInput(deps); }

void MenuScene::Draw(SettingsRuntimeRenderDeps &deps) const { DrawSettingsRuntime(deps); }

SettingsRuntimeLayout MakeSettingsRuntimeLayout(const LayoutMetrics &layout) {
  return SettingsRuntimeLayout{
      layout.screen_w,
      layout.screen_h,
      layout.top_bar_y,
      layout.top_bar_h,
      layout.bottom_bar_y,
      layout.bottom_bar_h,
      layout.settings_sidebar_w,
      layout.settings_y_offset,
      layout.settings_content_offset_y,
      layout.ui_scale,
  };
}

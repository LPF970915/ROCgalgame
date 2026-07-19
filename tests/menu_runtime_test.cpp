#include "game_settings_panel.h"
#include "menu_scene.h"
#include "system_controls_panel.h"

#include <SDL.h>

#include <cassert>
#include <filesystem>
#include <fstream>
#include <iostream>

namespace {
SDL_Event ControllerButtonEvent(Uint32 type, Uint8 button) {
  SDL_Event event{};
  event.type = type;
  event.cbutton.type = type;
  event.cbutton.button = button;
  return event;
}
}  // namespace

int main() {
  const std::filesystem::path mapping =
      std::filesystem::path("build") / "menu_runtime_empty_keymap.ini";
  std::filesystem::create_directories(mapping.parent_path());
  std::ofstream(mapping, std::ios::trunc) << "# no overrides\n";

  InputManager input(mapping.string(), InputProfile::DesktopDefault);
  MenuScene scene;
  SettingsRuntimeState state;
  state.items = {SettingId::KeyGuide, SettingId::SystemControls, SettingId::ExitApp};
  bool closed = false;
  bool exited = false;
  SettingId opened = SettingId::KeyGuide;
  int adjusted_row = -1;
  int adjusted_delta = 0;

  auto handle = [&]() {
    SettingsPanelInputHandlers handlers;
    handlers.system_controls = [&](SettingsRuntimeState &menu_state) {
      return HandleSystemControlsPanelInput(
          input, menu_state, [&](int row, int delta) {
            adjusted_row = row;
            adjusted_delta = delta;
          });
    };
    SettingsRuntimeInputActions actions;
    actions.animations_enabled = false;
    actions.on_close = [&]() { closed = true; };
    actions.on_exit_app = [&]() { exited = true; };
    actions.on_open_panel = [&](SettingId id, SettingsRuntimeState &) { opened = id; };
    actions.panel_handlers = std::move(handlers);
    SettingsRuntimeInputDeps deps{input, 0.016f, state, std::move(actions)};
    scene.HandleInput(deps);
  };

  auto button_frame = [&](Uint8 button) {
    input.BeginFrame(0.016f);
    input.HandleEvent(ControllerButtonEvent(SDL_CONTROLLERBUTTONDOWN, button));
    input.EndFrame();
    handle();
    input.BeginFrame(0.016f);
    input.HandleEvent(ControllerButtonEvent(SDL_CONTROLLERBUTTONUP, button));
    input.EndFrame();
    handle();
  };

  scene.BeginOpen(state, MenuSceneAnimationConfig{false, 0.0f, 0.0f, 0.0f});
  input.BeginFrame(0.016f);
  input.EndFrame();
  handle();
  assert(state.close_armed);
  assert(state.anim.Value() == 1.0f);

  button_frame(SDL_CONTROLLER_BUTTON_DPAD_DOWN);
  assert(state.selected == 1);
  button_frame(SDL_CONTROLLER_BUTTON_A);
  assert(state.panel_active);
  assert(opened == SettingId::SystemControls);

  button_frame(SDL_CONTROLLER_BUTTON_DPAD_RIGHT);
  assert(adjusted_row == 0);
  assert(adjusted_delta == 1);
  button_frame(SDL_CONTROLLER_BUTTON_B);
  assert(!state.panel_active);

  button_frame(SDL_CONTROLLER_BUTTON_DPAD_DOWN);
  assert(state.selected == 2);
  button_frame(SDL_CONTROLLER_BUTTON_A);
  assert(exited);

  state.selected = 0;
  button_frame(SDL_CONTROLLER_BUTTON_B);
  assert(closed);
  assert(state.anim.Value() == 0.0f);

  animation::TweenFloat tween(0.0f);
  tween.AnimateTo(1.0f, 0.2f, animation::Ease::OutCubic);
  tween.Update(0.1f);
  assert(tween.Value() > 0.0f && tween.Value() < 1.0f);
  tween.Update(0.1f);
  assert(tween.Value() == 1.0f);
  assert(!tween.IsAnimating());

  std::cout << "menu runtime tests passed\n";
  return 0;
}

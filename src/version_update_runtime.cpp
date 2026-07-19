#include "version_update_runtime.h"

bool HandleVersionUpdateInput(const InputManager &input, VersionUpdateState &state,
                              const VersionUpdateCallbacks &callbacks) {
  if (input.IsJustPressed(Button::B) || input.IsJustPressed(Button::Menu)) {
    if (callbacks.close_panel) callbacks.close_panel();
    return true;
  }
  if (input.IsJustPressed(Button::A) && callbacks.start_check) callbacks.start_check(state);
  return true;
}

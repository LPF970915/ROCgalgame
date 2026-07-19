#include "key_calibration_runtime.h"

#ifndef _WIN32
#include <linux/input.h>
#endif

#include <vector>

const std::array<Button, kKeyCalibrationButtonCount> &KeyCalibrationButtons() {
  static constexpr std::array<Button, kKeyCalibrationButtonCount> buttons = {{
      Button::Up, Button::Down, Button::Left, Button::Right,
      Button::A, Button::B, Button::X, Button::Y,
      Button::L1, Button::R1, Button::L2, Button::R2,
      Button::Start, Button::Select, Button::Menu,
  }};
  return buttons;
}

void StartKeyCalibration(InputManager &input, KeyCalibrationState &state, Uint32 now) {
  state = {};
  state.phase = KeyCalibrationPhase::Capturing;
  state.status = u8"请依次按下屏幕提示的实体按键";
  state.accept_after = now + 180;
  input.ClearCalibrationSamples();
  input.ResetAll();
}

bool SaveKeyCalibrationMapping(const std::filesystem::path &path,
                               const std::string &device_model,
                               InputManager &input,
                               const KeyCalibrationState &state) {
  std::vector<InputCalibrationMapping> mappings;
  mappings.reserve(KeyCalibrationButtons().size());
  for (size_t i = 0; i < KeyCalibrationButtons().size(); ++i) {
    mappings.push_back(InputCalibrationMapping{KeyCalibrationButtons()[i], state.bindings[i]});
  }
  return SaveInputCalibrationMappings(path.string(), device_model, mappings) &&
         input.ReloadMappings();
}

bool HandleKeyCalibrationInput(InputManager &input, KeyCalibrationState &state,
                               Uint32 now, const KeyCalibrationCallbacks &callbacks) {
  if (state.phase != KeyCalibrationPhase::Capturing) {
    if (input.IsJustPressed(Button::B) || input.IsJustPressed(Button::Menu)) {
      if (callbacks.close_panel) callbacks.close_panel();
      return true;
    }
    if (input.IsJustPressed(Button::A)) StartKeyCalibration(input, state, now);
    return true;
  }
  if (!SDL_TICKS_PASSED(now, state.accept_after)) return true;

  RawInputBinding sample;
  while (input.TakeCalibrationSample(sample)) {
    if (!sample.pressed) continue;
#ifndef _WIN32
    if (sample.source == RawInputSource::LinuxKey &&
        (sample.code == KEY_VOLUMEUP || sample.code == KEY_VOLUMEDOWN)) {
      continue;
    }
#endif
    state.bindings[static_cast<size_t>(state.current)] = sample;
    ++state.current;
    input.ClearCalibrationSamples();
    input.ResetAll();
    state.accept_after = now + 180;
    if (callbacks.play_capture_sfx) callbacks.play_capture_sfx();
    if (state.current >= static_cast<int>(KeyCalibrationButtons().size())) {
      if (callbacks.save_mapping && callbacks.save_mapping(state)) {
        state.phase = KeyCalibrationPhase::Complete;
        state.status = u8"校准完成，映射已立即生效";
      } else {
        state.phase = KeyCalibrationPhase::Failed;
        state.status = u8"校准保存失败，请检查配置目录权限";
      }
    }
    break;
  }
  return true;
}

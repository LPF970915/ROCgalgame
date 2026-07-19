#pragma once

#include "input_manager.h"

#include <SDL.h>

#include <array>
#include <filesystem>
#include <functional>
#include <string>

enum class KeyCalibrationPhase {
  Ready,
  Capturing,
  Complete,
  Failed,
};

constexpr size_t kKeyCalibrationButtonCount = 15;

struct KeyCalibrationState {
  KeyCalibrationPhase phase = KeyCalibrationPhase::Ready;
  int current = 0;
  Uint32 accept_after = 0;
  std::array<RawInputBinding, kKeyCalibrationButtonCount> bindings{};
  std::string status;
};

struct KeyCalibrationCallbacks {
  std::function<bool(const KeyCalibrationState &)> save_mapping;
  std::function<void()> play_capture_sfx;
  std::function<void()> close_panel;
};

const std::array<Button, kKeyCalibrationButtonCount> &KeyCalibrationButtons();
void StartKeyCalibration(InputManager &input, KeyCalibrationState &state, Uint32 now);
bool SaveKeyCalibrationMapping(const std::filesystem::path &path,
                               const std::string &device_model,
                               InputManager &input,
                               const KeyCalibrationState &state);
bool HandleKeyCalibrationInput(InputManager &input, KeyCalibrationState &state,
                               Uint32 now, const KeyCalibrationCallbacks &callbacks);

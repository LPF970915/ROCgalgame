#pragma once

#include "input_manager.h"

#include <functional>

enum class VersionUpdateStatus {
  Idle,
  Unconfigured,
  Checking,
};

struct VersionUpdateState {
  VersionUpdateStatus status = VersionUpdateStatus::Idle;
};

struct VersionUpdateCallbacks {
  std::function<void()> close_panel;
  std::function<void(VersionUpdateState &)> start_check;
};

bool HandleVersionUpdateInput(const InputManager &input, VersionUpdateState &state,
                              const VersionUpdateCallbacks &callbacks);

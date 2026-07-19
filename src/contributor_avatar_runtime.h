#pragma once

#include "input_manager.h"
#include "settings_runtime.h"

#include <filesystem>
#include <functional>
#include <string>
#include <vector>

struct ContributorAvatarEntry {
  std::filesystem::path path;
  std::string filename;
  double contribution = 0.0;
  bool contribution_is_max = false;
};

struct ContributorAvatarState {
  int focus_index = 0;
  int scroll_row = 0;
  int marquee_focus_index = -1;
  float marquee_wait = 0.0f;
  float marquee_offset = 0.0f;
  uint32_t marquee_last_tick_ms = 0;
};

bool HandleContributorAvatarInput(
    const InputManager &input, float dt, SettingsRuntimeState &menu_state,
    ContributorAvatarState &state, size_t entry_count,
    const std::function<void(int)> &on_confirm);

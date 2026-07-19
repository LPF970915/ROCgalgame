#pragma once

#include "game_library_service.h"
#include "input_manager.h"

#include <functional>
#include <vector>

struct ShelfRuntimeState {
  std::vector<int> visible_games;
  int focus_index = 0;
  int page = 0;
  int category_index = 0;
  int previous_page = 0;
  int page_direction = 0;
  float page_animation = 1.0f;
  int title_focus_index = -1;
  float title_marquee_wait = 0.0f;
  float title_marquee_offset = 0.0f;
};

struct ShelfRuntimeCallbacks {
  std::function<void()> open_menu;
  std::function<void(const std::string &)> show_message;
  std::function<void(const GameEntry &)> launch_game;
};

void RebuildShelfItems(ShelfRuntimeState &state,
                       const GameLibraryService &library, int grid_cols);
void ChangeShelfCategory(ShelfRuntimeState &state,
                         const GameLibraryService &library, int delta,
                         int grid_cols, int category_count = 4);
void MoveShelfFocus(ShelfRuntimeState &state, int dx, int dy, int grid_cols);
const GameEntry *FocusedShelfGame(const ShelfRuntimeState &state,
                                  const GameLibraryService &library);
void TickShelfRuntime(ShelfRuntimeState &state, float dt, bool title_needs_marquee,
                      float marquee_speed = 48.0f);
void HandleShelfRuntimeInput(const InputManager &input, ShelfRuntimeState &state,
                             GameLibraryService &library, int grid_cols,
                             const ShelfRuntimeCallbacks &callbacks);

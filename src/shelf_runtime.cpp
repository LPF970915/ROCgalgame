#include "shelf_runtime.h"

#include <algorithm>

namespace {
void ClampShelf(ShelfRuntimeState &state, int grid_cols) {
  const int count = static_cast<int>(state.visible_games.size());
  grid_cols = std::max(1, grid_cols);
  if (count <= 0) {
    state.focus_index = 0;
    state.page = 0;
    return;
  }
  state.focus_index = std::clamp(state.focus_index, 0, count - 1);
  const int max_page = (count - 1) / grid_cols;
  state.page = std::clamp(state.page, 0, max_page);
  const int col = std::clamp(state.focus_index % grid_cols, 0, grid_cols - 1);
  state.focus_index = std::min(count - 1, state.page * grid_cols + col);
}

void StartPageAnimation(ShelfRuntimeState &state, int old_page) {
  if (old_page == state.page) return;
  state.previous_page = old_page;
  state.page_direction = state.page > old_page ? 1 : -1;
  state.page_animation = 0.0f;
}

void ResetTitleMarquee(ShelfRuntimeState &state) {
  state.title_focus_index = state.focus_index;
  state.title_marquee_wait = 0.0f;
  state.title_marquee_offset = 0.0f;
}
}  // namespace

void RebuildShelfItems(ShelfRuntimeState &state,
                       const GameLibraryService &library, int grid_cols) {
  state.visible_games = library.VisibleIndices(state.category_index);
  ClampShelf(state, grid_cols);
  ResetTitleMarquee(state);
}

void ChangeShelfCategory(ShelfRuntimeState &state,
                         const GameLibraryService &library, int delta,
                         int grid_cols, int category_count) {
  category_count = std::max(1, category_count);
  state.category_index = (state.category_index + (delta < 0 ? -1 : 1) + category_count) %
                         category_count;
  state.focus_index = 0;
  state.page = 0;
  state.previous_page = 0;
  state.page_animation = 1.0f;
  RebuildShelfItems(state, library, grid_cols);
}

void MoveShelfFocus(ShelfRuntimeState &state, int dx, int dy, int grid_cols) {
  const int count = static_cast<int>(state.visible_games.size());
  if (count <= 0) return;
  grid_cols = std::max(1, grid_cols);
  const int old_page = state.page;
  int col = state.focus_index % grid_cols;
  const int max_page = (count - 1) / grid_cols;
  if (dx < 0) {
    if (col > 0) --col;
    else if (state.page > 0) {
      --state.page;
      col = grid_cols - 1;
    }
  } else if (dx > 0) {
    if (col < grid_cols - 1) ++col;
    else if (state.page < max_page) {
      ++state.page;
      col = 0;
    }
  }
  if (dy < 0 && state.page > 0) --state.page;
  if (dy > 0 && state.page < max_page) ++state.page;
  state.page = std::clamp(state.page, 0, max_page);
  state.focus_index = std::min(count - 1, state.page * grid_cols + col);
  StartPageAnimation(state, old_page);
  ResetTitleMarquee(state);
}

const GameEntry *FocusedShelfGame(const ShelfRuntimeState &state,
                                  const GameLibraryService &library) {
  return library.GameAtVisibleIndex(state.visible_games, state.focus_index);
}

void TickShelfRuntime(ShelfRuntimeState &state, float dt, bool title_needs_marquee,
                      float marquee_speed) {
  if (state.page_animation < 1.0f) {
    state.page_animation = std::min(1.0f, state.page_animation + dt / 0.18f);
  }
  if (state.title_focus_index != state.focus_index) ResetTitleMarquee(state);
  if (!title_needs_marquee) {
    state.title_marquee_wait = 0.0f;
    state.title_marquee_offset = 0.0f;
    return;
  }
  state.title_marquee_wait += dt;
  if (state.title_marquee_wait > 0.8f) {
    state.title_marquee_offset += std::max(1.0f, marquee_speed) * dt;
  }
}

void HandleShelfRuntimeInput(const InputManager &input, ShelfRuntimeState &state,
                             GameLibraryService &library, int grid_cols,
                             const ShelfRuntimeCallbacks &callbacks) {
  if (input.Pressed(Button::Menu)) {
    if (callbacks.open_menu) callbacks.open_menu();
    return;
  }
  if (input.Pressed(Button::L1)) ChangeShelfCategory(state, library, -1, grid_cols);
  if (input.Pressed(Button::R1)) ChangeShelfCategory(state, library, 1, grid_cols);
  if (input.Pressed(Button::Left)) MoveShelfFocus(state, -1, 0, grid_cols);
  if (input.Pressed(Button::Right)) MoveShelfFocus(state, 1, 0, grid_cols);
  if (input.Pressed(Button::Up)) MoveShelfFocus(state, 0, -1, grid_cols);
  if (input.Pressed(Button::Down)) MoveShelfFocus(state, 0, 1, grid_cols);
  const GameEntry *game = FocusedShelfGame(state, library);
  if (!game) return;
  if (input.Pressed(Button::X) && library.AddFavorite(*game)) {
    if (callbacks.show_message) callbacks.show_message(u8"已加入收藏");
    if (state.category_index == 2) RebuildShelfItems(state, library, grid_cols);
  }
  if (input.Pressed(Button::Y) && library.RemoveFavorite(*game)) {
    if (callbacks.show_message) callbacks.show_message(u8"已移出收藏");
    if (state.category_index == 2) RebuildShelfItems(state, library, grid_cols);
  }
  if (input.Pressed(Button::A)) {
    library.PushHistory(*game);
    if (callbacks.launch_game) callbacks.launch_game(*game);
  }
}

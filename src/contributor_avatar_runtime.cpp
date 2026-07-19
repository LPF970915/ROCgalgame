#include "contributor_avatar_runtime.h"

#include <algorithm>

bool HandleContributorAvatarInput(
    const InputManager &input, SettingsRuntimeState &menu_state,
    ContributorAvatarState &state, size_t entry_count,
    const std::function<void(int)> &on_confirm) {
  if (input.IsJustPressed(Button::B) || input.IsJustPressed(Button::Menu)) {
    menu_state.panel_active = false;
    return true;
  }
  if (entry_count == 0) return true;
  state.focus_index = std::clamp(state.focus_index, 0, static_cast<int>(entry_count) - 1);
  int row = state.focus_index / 3;
  int col = state.focus_index % 3;
  if ((input.IsJustPressed(Button::Left) || input.IsRepeated(Button::Left)) && col > 0) --col;
  else if ((input.IsJustPressed(Button::Right) || input.IsRepeated(Button::Right)) &&
           state.focus_index + 1 < static_cast<int>(entry_count) && col < 2) ++col;
  else if ((input.IsJustPressed(Button::Up) || input.IsRepeated(Button::Up)) && row > 0) --row;
  else if ((input.IsJustPressed(Button::Down) || input.IsRepeated(Button::Down)) &&
           (row + 1) * 3 < static_cast<int>(entry_count)) ++row;
  state.focus_index = std::min(static_cast<int>(entry_count) - 1, row * 3 + col);
  const int focus_row = state.focus_index / 3;
  if (focus_row < state.scroll_row) state.scroll_row = focus_row;
  if (focus_row > state.scroll_row + 1) state.scroll_row = focus_row - 1;
  if (input.IsJustPressed(Button::A) && on_confirm) on_confirm(state.focus_index);
  return true;
}

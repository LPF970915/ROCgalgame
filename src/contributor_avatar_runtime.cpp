#include "contributor_avatar_runtime.h"

#include <algorithm>
#include <cmath>

namespace {
constexpr float kAvatarNameMarqueePauseSec = 0.0f;
constexpr float kAvatarNameMarqueeSpeedPx = 84.0f;

void ResetMarquee(ContributorAvatarState &state) {
  state.marquee_focus_index = state.focus_index;
  state.marquee_wait = kAvatarNameMarqueePauseSec;
  state.marquee_offset = 0.0f;
  state.marquee_last_tick_ms = SDL_GetTicks();
}

void TickMarquee(ContributorAvatarState &state, float dt) {
  if (state.focus_index != state.marquee_focus_index) {
    ResetMarquee(state);
    return;
  }
  const uint32_t now = SDL_GetTicks();
  float effective_dt = dt;
  if (state.marquee_last_tick_ms != 0) {
    const float tick_dt = static_cast<float>(now - state.marquee_last_tick_ms) / 1000.0f;
    if (tick_dt > 0.0f) effective_dt = tick_dt;
  }
  state.marquee_last_tick_ms = now;
  effective_dt = std::clamp(effective_dt, 0.0f, 0.05f);
  if (state.marquee_wait > 0.0f) {
    state.marquee_wait = std::max(0.0f, state.marquee_wait - effective_dt);
  }
  if (state.marquee_wait <= 0.0f) {
    state.marquee_offset += kAvatarNameMarqueeSpeedPx * effective_dt;
    if (state.marquee_offset > 8192.0f) {
      state.marquee_offset = std::fmod(state.marquee_offset, 8192.0f);
    }
  }
}
}  // namespace

bool HandleContributorAvatarInput(
    const InputManager &input, float dt, SettingsRuntimeState &menu_state,
    ContributorAvatarState &state, size_t entry_count,
    const std::function<void(int)> &on_confirm) {
  if (input.IsJustPressed(Button::B) || input.IsJustPressed(Button::Menu)) {
    menu_state.panel_active = false;
    state.marquee_last_tick_ms = SDL_GetTicks();
    return true;
  }
  if (entry_count == 0) {
    state.focus_index = 0;
    state.scroll_row = 0;
    state.marquee_focus_index = -1;
    state.marquee_wait = 0.0f;
    state.marquee_offset = 0.0f;
    state.marquee_last_tick_ms = 0;
    return true;
  }
  state.focus_index = std::clamp(state.focus_index, 0, static_cast<int>(entry_count) - 1);
  TickMarquee(state, dt);
  const int previous_focus = state.focus_index;
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
  const int total_rows = static_cast<int>((entry_count + 2) / 3);
  state.scroll_row = std::clamp(state.scroll_row, 0, std::max(0, total_rows - 2));
  if (state.focus_index != previous_focus) ResetMarquee(state);
  if (input.IsJustPressed(Button::A) && on_confirm) on_confirm(state.focus_index);
  return true;
}

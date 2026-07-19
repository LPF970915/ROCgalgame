#include "power_lifecycle.h"

#include <SDL.h>

PowerLifecycleController::PowerLifecycleController(InputProfile input_profile,
                                                   LidPowerController &display)
    : PowerLifecycleController(
          input_profile,
          [&display, input_profile]() { return display.TriggerPowerKeyScreenOff(input_profile); },
          [&display, input_profile]() {
            return input_profile == InputProfile::GKD350HUltra
                       ? display.TriggerPowerKeyScreenOff(input_profile)
                       : display.TriggerAutoIfEnabled();
          },
          [&display, input_profile]() { return display.TriggerScreenOn(input_profile); }) {}

PowerLifecycleController::PowerLifecycleController(InputProfile input_profile,
                                                   DisplayCallback turn_off,
                                                   DisplayCallback turn_on)
    : input_profile_(input_profile), turn_off_(std::move(turn_off)),
      turn_auto_off_(turn_off_), turn_on_(std::move(turn_on)) {}

PowerLifecycleController::PowerLifecycleController(InputProfile input_profile,
                                                   DisplayCallback turn_off,
                                                   DisplayCallback turn_auto_off,
                                                   DisplayCallback turn_on)
    : input_profile_(input_profile), turn_off_(std::move(turn_off)),
      turn_auto_off_(std::move(turn_auto_off)), turn_on_(std::move(turn_on)) {}

ScreenOffMode PowerLifecycleController::Mode() const { return mode_; }
bool PowerLifecycleController::IsScreenOff() const { return mode_ != ScreenOffMode::Awake; }
uint32_t PowerLifecycleController::LastActivityTick() const { return last_activity_tick_; }

void PowerLifecycleController::Initialize(uint32_t now) {
  mode_ = ScreenOffMode::Awake;
  last_activity_tick_ = now;
  power_ignore_until_tick_ = 0;
  deferred_wake_tick_ = 0;
}

void PowerLifecycleController::NoteActivity(uint32_t now) {
  if (mode_ == ScreenOffMode::Awake) last_activity_tick_ = now;
}

PowerLifecycleResult PowerLifecycleController::WakeNow(uint32_t now) {
  PowerLifecycleResult result;
  result.consumed_input = true;
  if (!turn_on_ || !turn_on_()) return result;
  mode_ = ScreenOffMode::Awake;
  last_activity_tick_ = now;
  deferred_wake_tick_ = 0;
  power_ignore_until_tick_ = now + 900;
  result.display_turned_on = true;
  result.refresh_input_devices = true;
  return result;
}

PowerLifecycleResult PowerLifecycleController::Update(
    uint32_t now, bool auto_sleep_enabled, uint32_t auto_sleep_interval_ms,
    bool power_pressed, bool non_power_activity) {
  PowerLifecycleResult result;

  if (deferred_wake_tick_ != 0 && SDL_TICKS_PASSED(now, deferred_wake_tick_)) {
    return WakeNow(now);
  }

  const bool power_allowed = SDL_TICKS_PASSED(now, power_ignore_until_tick_);
  if (mode_ != ScreenOffMode::Awake) {
    result.consumed_input = power_pressed || non_power_activity || deferred_wake_tick_ != 0;
    if (power_pressed && power_allowed) {
      if (input_profile_ == InputProfile::GKD350HUltra) {
        deferred_wake_tick_ = now + 650;
        power_ignore_until_tick_ = now + 1200;
        result.consumed_input = true;
        return result;
      }
      return WakeNow(now);
    }
    if (mode_ == ScreenOffMode::Auto && non_power_activity) return WakeNow(now);
    return result;
  }

  if (non_power_activity) last_activity_tick_ = now;
  if (power_pressed && power_allowed) {
    result.consumed_input = true;
    if (turn_off_ && turn_off_()) {
      mode_ = ScreenOffMode::Manual;
      deferred_wake_tick_ = 0;
      result.display_turned_off = true;
    }
    return result;
  }

  if (auto_sleep_enabled && auto_sleep_interval_ms > 0 &&
      now - last_activity_tick_ >= auto_sleep_interval_ms) {
    if (turn_auto_off_ && turn_auto_off_()) {
      mode_ = ScreenOffMode::Auto;
      result.display_turned_off = true;
      result.consumed_input = true;
    } else {
      last_activity_tick_ = now;
    }
  }
  return result;
}

PowerLifecycleResult PowerLifecycleController::EnsureScreenOn(uint32_t now) {
  if (mode_ == ScreenOffMode::Awake) return {};
  return WakeNow(now);
}

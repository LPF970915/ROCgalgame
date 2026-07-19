#pragma once

#include "input_manager.h"
#include "lid_power_control.h"

#include <cstdint>
#include <functional>

enum class ScreenOffMode { Awake, Manual, Auto };

struct PowerLifecycleResult {
  bool consumed_input = false;
  bool display_turned_off = false;
  bool display_turned_on = false;
  bool refresh_input_devices = false;
};

class PowerLifecycleController {
public:
  using DisplayCallback = std::function<bool()>;

  PowerLifecycleController(InputProfile input_profile, LidPowerController &display);
  PowerLifecycleController(InputProfile input_profile, DisplayCallback turn_off,
                           DisplayCallback turn_on);
  PowerLifecycleController(InputProfile input_profile, DisplayCallback turn_off,
                           DisplayCallback turn_auto_off, DisplayCallback turn_on);

  ScreenOffMode Mode() const;
  bool IsScreenOff() const;
  uint32_t LastActivityTick() const;
  void Initialize(uint32_t now);
  void NoteActivity(uint32_t now);
  PowerLifecycleResult Update(uint32_t now, bool auto_sleep_enabled,
                              uint32_t auto_sleep_interval_ms, bool power_pressed,
                              bool non_power_activity);
  PowerLifecycleResult EnsureScreenOn(uint32_t now);

private:
  PowerLifecycleResult WakeNow(uint32_t now);

  InputProfile input_profile_ = InputProfile::DesktopDefault;
  DisplayCallback turn_off_;
  DisplayCallback turn_auto_off_;
  DisplayCallback turn_on_;
  ScreenOffMode mode_ = ScreenOffMode::Awake;
  uint32_t last_activity_tick_ = 0;
  uint32_t power_ignore_until_tick_ = 0;
  uint32_t deferred_wake_tick_ = 0;
};

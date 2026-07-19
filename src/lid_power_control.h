#pragma once

#include "filesystem_compat.h"
#include "input_manager.h"

#include <string>

class LidPowerController {
public:
  explicit LidPowerController(std::filesystem::path power_script_path = {});

  bool TriggerAutoIfEnabled() const;
  bool TriggerPowerKeyScreenOff(InputProfile input_profile) const;
  bool TriggerScreenOn(InputProfile input_profile) const;
  std::string PowerScriptPath() const;

private:
  std::filesystem::path power_script_path_;
};

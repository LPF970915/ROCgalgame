#pragma once

#include "game_core_adapter.h"

#include <functional>

struct LaunchResult {
  LaunchStatus status = LaunchStatus::Unsupported;
  int exit_code = -1;
  int signal = 0;
  std::filesystem::path log_path;
  std::string detail;
};

using CorePollCallback = std::function<bool()>;

class ICoreProcessRunner {
public:
  virtual ~ICoreProcessRunner() = default;
  virtual LaunchResult Run(const CoreLaunchSpec &spec,
                           const CorePollCallback &poll = {}) const = 0;
};

class CoreProcessRunner final : public ICoreProcessRunner {
public:
  LaunchResult Run(const CoreLaunchSpec &spec,
                   const CorePollCallback &poll = {}) const override;
};

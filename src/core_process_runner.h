#pragma once

#include "game_core_adapter.h"

struct LaunchResult {
  LaunchStatus status = LaunchStatus::Unsupported;
  int exit_code = -1;
  int signal = 0;
  std::filesystem::path log_path;
  std::string detail;
};

class ICoreProcessRunner {
public:
  virtual ~ICoreProcessRunner() = default;
  virtual LaunchResult Run(const CoreLaunchSpec &spec) const = 0;
};

class CoreProcessRunner final : public ICoreProcessRunner {
public:
  LaunchResult Run(const CoreLaunchSpec &spec) const override;
};

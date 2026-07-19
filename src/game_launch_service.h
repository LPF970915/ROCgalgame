#pragma once

#include "core_process_runner.h"
#include "game_core_registry.h"

class GameLaunchService {
public:
  GameLaunchService(const GameCoreRegistry &registry,
                    const ICoreProcessRunner &runner)
      : registry_(registry), runner_(runner) {}

  CoreSpecResult BuildSpec(const AppConfig &config,
                           const GameEntry &game) const;
  LaunchResult Launch(const AppConfig &config, const GameEntry &game) const;

private:
  const GameCoreRegistry &registry_;
  const ICoreProcessRunner &runner_;
};

std::string DescribeLaunchResult(const LaunchResult &result, int language_index = 0);

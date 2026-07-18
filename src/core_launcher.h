#pragma once

#include "config.h"
#include "game_library.h"

#include <filesystem>
#include <map>
#include <string>
#include <vector>

enum class LaunchStatus {
  NormalExit,
  MissingCore,
  InvalidGame,
  ExecFailure,
  CoreError,
  Signaled,
  Unsupported,
};

struct CoreLaunchSpec {
  CoreKind core = CoreKind::Unknown;
  std::filesystem::path executable;
  std::filesystem::path working_directory;
  std::filesystem::path entry_point;
  std::filesystem::path save_path;
  std::filesystem::path log_path;
  std::vector<std::string> arguments;
  std::map<std::string, std::string> environment;
};

struct LaunchResult {
  LaunchStatus status = LaunchStatus::Unsupported;
  int exit_code = -1;
  int signal = 0;
  std::filesystem::path log_path;
};

bool BuildCoreLaunchSpec(const AppConfig &config, const GameEntry &game, CoreLaunchSpec &spec);
LaunchResult LaunchGameAndWait(const AppConfig &config, const GameEntry &game);
std::string DescribeLaunchResult(const LaunchResult &result);

// Legacy recovery protocol used only by old packages whose shell owns core launch.
bool WriteLaunchRequest(const AppConfig &config, const GameEntry &game);

#pragma once

#include "config.h"
#include "game_library.h"
#include "game_settings_snapshot.h"

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

struct CoreSpecResult {
  LaunchStatus status = LaunchStatus::Unsupported;
  CoreLaunchSpec spec;
  std::string detail;

  bool Ok() const { return status == LaunchStatus::NormalExit; }
};

struct EffectiveGameSettings {
  std::string aspect;
  std::string filter;
  bool virtual_mouse = true;
  int mouse_speed = 720;
  float mouse_acceleration = 1.6f;
};

class IGameCoreAdapter {
public:
  virtual ~IGameCoreAdapter() = default;
  virtual CoreKind Kind() const = 0;
  virtual CoreSpecResult BuildSpec(const AppConfig &config,
                                   const GameEntry &game) const = 0;
};

EffectiveGameSettings ResolveEffectiveGameSettings(const AppConfig &config,
                                                   const GameEntry &game);
CoreLaunchSpec MakeBaseCoreLaunchSpec(const AppConfig &config,
                                      const GameEntry &game,
                                      const EffectiveGameSettings &settings);
std::filesystem::path PreferredGameFont(const AppConfig &config,
                                        const GameEntry &game);

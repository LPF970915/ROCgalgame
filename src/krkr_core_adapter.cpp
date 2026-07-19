#include "krkr_core_adapter.h"

#include <system_error>

namespace {
#ifdef _WIN32
std::filesystem::path ExistingExecutable(
    const std::vector<std::filesystem::path> &candidates) {
  std::error_code ec;
  for (const auto &candidate : candidates) {
    if (std::filesystem::is_regular_file(candidate, ec)) return candidate;
    ec.clear();
  }
  return candidates.empty() ? std::filesystem::path{} : candidates.front();
}
#else
std::string PluginSearchPath(const AppConfig &config, const GameEntry &game) {
  return (game.path / "plugin").u8string() + ":" +
         (config.root / "cores" / "krkr" / "plugin").u8string() + ":" +
         (config.root / "plugin").u8string();
}
#endif
}  // namespace

CoreSpecResult KrkrCoreAdapter::BuildSpec(const AppConfig &config,
                                          const GameEntry &game) const {
  if (game.core != Kind()) return CoreSpecResult{LaunchStatus::Unsupported, {}, "core mismatch"};
  std::error_code ec;
  if (!std::filesystem::is_directory(game.path, ec)) {
    return CoreSpecResult{LaunchStatus::InvalidGame, {}, "game directory missing"};
  }
  const EffectiveGameSettings settings = ResolveEffectiveGameSettings(config, game);
  CoreLaunchSpec spec = MakeBaseCoreLaunchSpec(config, game, settings);
#ifdef _WIN32
  spec.executable = ExistingExecutable({config.root / "cores" / "krkr" / "tvpwin64.exe",
                                        config.root / "cores" / "krkr" / "krkrsdl2.exe"});
#else
  spec.executable = config.root / "cores" / "krkr" / "krkrsdl2";
#endif
  spec.entry_point = game.entry_point.empty() ? game.path : game.entry_point;
  if (!std::filesystem::exists(spec.entry_point, ec)) {
    return CoreSpecResult{LaunchStatus::InvalidGame, {}, "entry point missing"};
  }
  const int frame_limit = game.overrides.frame_limit > 0
                              ? game.overrides.frame_limit : 60;
  const std::string draw_threads = game.overrides.draw_threads.empty()
                                       ? "auto" : game.overrides.draw_threads;
  const int graphic_cache_mb = game.overrides.graphic_cache_mb > 0
                                   ? game.overrides.graphic_cache_mb : 96;
  const std::filesystem::path font = PreferredGameFont(config, game);
  spec.arguments = {spec.executable.u8string(), spec.entry_point.u8string(),
                    "-datapath=" + spec.save_path.u8string(),
                    "-contfreq=" + std::to_string(frame_limit),
                    "-drawthread=" + draw_threads,
                    "-gclim=" + std::to_string(graphic_cache_mb),
                    "-deffont=" + font.u8string(), "-nosel"};
  spec.environment["ROCGALGAME_KRKR_VIRTUAL_MOUSE"] =
      settings.virtual_mouse ? "1" : "0";
  spec.environment["ROCGALGAME_KRKR_SWAP_AB"] = "1";
  spec.environment["ROCGALGAME_KRKR_CONTINUOUS_PRESENT"] = "1";
  spec.environment["AETHERKIRI_MOTION_DEBUG"] = "1";
#ifndef _WIN32
  spec.environment["KRKRSDL2_PATH"] = PluginSearchPath(config, game);
#endif
  return CoreSpecResult{LaunchStatus::NormalExit, std::move(spec), {}};
}

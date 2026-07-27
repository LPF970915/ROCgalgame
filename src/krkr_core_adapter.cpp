#include "krkr_core_adapter.h"

#include <cstdlib>
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

std::string Krkr2LibrarySearchPath(const AppConfig &config,
                                   bool native_wayland) {
  std::string path;
  if (native_wayland) path = "/usr/lib/mali:";
  path += (config.root / "cores" / "krkr" / "lib_krkr2").u8string() + ":" +
                     (config.root / "lib_system_sdl").u8string() + ":" +
                     (config.root / "lib").u8string() +
                     ":/usr/lib:/lib:/mnt/vendor/lib";
  if (!native_wayland) path += ":/usr/lib/mali";
  if (const char *inherited = std::getenv("LD_LIBRARY_PATH"); inherited && *inherited) {
    path += ":";
    path += inherited;
  }
  return path;
}

std::string InheritedOrDefault(const char *name, std::string fallback) {
  if (const char *value = std::getenv(name); value && *value) return value;
  return fallback;
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
  const KrkrRuntime runtime = game.overrides.krkr_runtime == KrkrRuntime::Auto
                                  ? KrkrRuntime::Sdl2
                                  : game.overrides.krkr_runtime;
  spec.environment["ROCGALGAME_KRKR_RUNTIME"] = KrkrRuntimeName(runtime);
  spec.environment["ROCGALGAME_KRKR_SAVE_PATH"] = spec.save_path.u8string();
#ifdef _WIN32
  if (runtime == KrkrRuntime::Wine) {
    return CoreSpecResult{LaunchStatus::Unsupported, std::move(spec),
                          "Wine KRKR backend is not installed"};
  }
  if (runtime == KrkrRuntime::Krkr2) {
    spec.executable = config.root / "cores" / "krkr" / "krkr2.exe";
  } else {
    spec.executable = ExistingExecutable({config.root / "cores" / "krkr" / "tvpwin64.exe",
                                          config.root / "cores" / "krkr" / "krkrsdl2.exe"});
  }
#else
  if (runtime == KrkrRuntime::Wine) {
    return CoreSpecResult{LaunchStatus::Unsupported, std::move(spec),
                          "Wine KRKR backend is not installed"};
  }
  spec.executable = config.root / "cores" / "krkr" /
                    (runtime == KrkrRuntime::Krkr2 ? "krkr2" : "krkrsdl2");
#endif
  spec.entry_point = game.entry_point.empty() ? game.path : game.entry_point;
  if (runtime == KrkrRuntime::Krkr2 && std::filesystem::is_directory(spec.entry_point, ec)) {
    // The native KrKr2 Linux host accepts a project archive or startup script,
    // but it does not mount a game directory as an XP3 project. Prefer the
    // conventional data.xp3 when a game was scanned without an explicit entry.
    const auto data_archive = spec.entry_point / "data.xp3";
    if (std::filesystem::is_regular_file(data_archive, ec))
      spec.entry_point = data_archive;
  }
  if (!std::filesystem::exists(spec.entry_point, ec)) {
    return CoreSpecResult{LaunchStatus::InvalidGame, {}, "entry point missing"};
  }
  const std::filesystem::path font = PreferredGameFont(config, game);
  if (runtime == KrkrRuntime::Krkr2) {
    // krkr2's Linux host consumes the project path as argv[1].
    spec.arguments = {spec.executable.u8string(), spec.entry_point.u8string()};
#ifndef _WIN32
    const std::string requested_backend =
        InheritedOrDefault("ROCGALGAME_KRKR_DISPLAY_BACKEND", "wayland");
    const std::string display_backend =
        requested_backend == "xwayland" ? "xwayland" : "wayland";
    spec.environment["ROCGALGAME_KRKR_DISPLAY_BACKEND"] = display_backend;
    spec.environment["ROCGALGAME_KRKR_XWAYLAND_WIDTH"] =
        InheritedOrDefault("ROCGALGAME_KRKR_XWAYLAND_WIDTH",
                           std::to_string(config.screen_w));
    spec.environment["ROCGALGAME_KRKR_XWAYLAND_HEIGHT"] =
        InheritedOrDefault("ROCGALGAME_KRKR_XWAYLAND_HEIGHT",
                           std::to_string(config.screen_h));
    if (display_backend == "xwayland") {
      spec.environment["ROCGALGAME_KRKR_XWAYLAND"] = "1";
      spec.environment["ROCGALGAME_KRKR_XWAYLAND_RENDERER"] =
          InheritedOrDefault("ROCGALGAME_KRKR_XWAYLAND_RENDERER", "hardware");
      spec.environment["ROCGALGAME_KRKR_XWAYLAND_SHM"] =
          InheritedOrDefault("ROCGALGAME_KRKR_XWAYLAND_SHM", "1");
      spec.environment["DISPLAY"] = ":2";
      spec.environment["GDK_BACKEND"] = "x11";
      spec.environment["SDL_VIDEODRIVER"] = "x11";
    } else {
      spec.environment["ROCGALGAME_KRKR_XWAYLAND"] = "0";
      spec.environment["XDG_RUNTIME_DIR"] =
          InheritedOrDefault("XDG_RUNTIME_DIR", "/run/0-runtime-dir");
      spec.environment["WAYLAND_DISPLAY"] =
          InheritedOrDefault("WAYLAND_DISPLAY", "wayland-1");
      spec.environment["SWAYSOCK"] = InheritedOrDefault(
          "SWAYSOCK", "/run/0-runtime-dir/sway-ipc.0.sock");
      spec.environment["GDK_BACKEND"] = "wayland";
      spec.environment["SDL_VIDEODRIVER"] = "wayland";
    }
    spec.environment["LD_LIBRARY_PATH"] =
        Krkr2LibrarySearchPath(config, display_backend != "xwayland");
#endif
  } else {
    const int frame_limit = game.overrides.frame_limit > 0
                                ? game.overrides.frame_limit : 60;
    const std::string draw_threads = game.overrides.draw_threads.empty()
                                         ? "auto" : game.overrides.draw_threads;
    const int graphic_cache_mb = game.overrides.graphic_cache_mb > 0
                                     ? game.overrides.graphic_cache_mb : 96;
    spec.arguments = {spec.executable.u8string(), spec.entry_point.u8string(),
                      "-datapath=" + spec.save_path.u8string(),
                      "-contfreq=" + std::to_string(frame_limit),
                      "-drawthread=" + draw_threads,
                      "-gclim=" + std::to_string(graphic_cache_mb),
                      "-deffont=" + font.u8string(), "-nosel"};
  }
  spec.environment["ROCGALGAME_KRKR_VIRTUAL_MOUSE"] =
      settings.virtual_mouse ? "1" : "0";
  spec.environment["ROCGALGAME_KRKR_SWAP_AB"] = "1";
  spec.environment["ROCGALGAME_INPUT_PROFILE"] = config.input_profile;
  int pointer_speed = settings.mouse_speed;
  float pointer_acceleration = settings.mouse_acceleration;
  if (runtime == KrkrRuntime::Krkr2 && game.overrides.mouse_speed <= 0 &&
      game.overrides.mouse_accel <= 0.0f && pointer_speed == 720 &&
      pointer_acceleration > 1.59f && pointer_acceleration < 1.61f) {
    // Migrate the legacy packaged defaults without overriding deliberate
    // per-game settings or changing the established SDL2 core behavior.
    pointer_speed = 1080;
    pointer_acceleration = 1.0f;
  }
  spec.environment["ROCGALGAME_MOUSE_SPEED"] = std::to_string(pointer_speed);
  spec.environment["ROCGALGAME_MOUSE_ACCEL"] =
      std::to_string(pointer_acceleration);
#ifndef _WIN32
  spec.environment["KRKRSDL2_PATH"] = PluginSearchPath(config, game);
#endif
  return CoreSpecResult{LaunchStatus::NormalExit, std::move(spec), {}};
}

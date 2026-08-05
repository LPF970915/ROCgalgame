#include "krkr_core_adapter.h"

#include <array>
#include <cstdint>
#include <cstdlib>
#include <iomanip>
#include <sstream>
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

bool IsLegacySaveDirectory(const std::filesystem::path &path) {
  const std::string name = ToLowerAscii(path.filename().u8string());
  return name == "savedata" || name == "save" || name == "saves" ||
         name == "strsave";
}

bool EnsureProjectViewLink(const std::filesystem::path &source,
                           const std::filesystem::path &link,
                           bool directory, std::error_code &ec) {
  const auto status = std::filesystem::symlink_status(link, ec);
  if (ec == std::errc::no_such_file_or_directory)
    ec.clear();
  else if (ec)
    return false;
  if (status.type() != std::filesystem::file_type::not_found) {
    if (!std::filesystem::is_symlink(status)) return true;
    const auto current = std::filesystem::read_symlink(link, ec);
    if (ec) return false;
    if (current == source) return true;
    std::filesystem::remove(link, ec);
    if (ec) return false;
  }
  if (directory)
    std::filesystem::create_directory_symlink(source, link, ec);
  else
    std::filesystem::create_symlink(source, link, ec);
  return !ec;
}

std::string StableProjectViewId(const std::filesystem::path &game_path) {
  std::uint64_t hash = 1469598103934665603ull;
  for (unsigned char ch : game_path.u8string()) {
    hash ^= ch;
    hash *= 1099511628211ull;
  }
  std::ostringstream value;
  value << std::hex << std::setfill('0') << std::setw(16) << hash;
  return value.str();
}

std::filesystem::path ProjectViewRoot() {
  if (const char *configured =
          std::getenv("ROCGALGAME_KRKR_PROJECT_VIEW_ROOT");
      configured && *configured)
    return std::filesystem::u8path(configured);
  return "/tmp/rocgalgame-krkr2-projects";
}

bool PrepareKrkr2ProjectView(CoreLaunchSpec &spec, const GameEntry &game,
                            std::error_code &ec) {
  const auto view = ProjectViewRoot() / StableProjectViewId(game.path);
  std::filesystem::create_directories(view, ec);
  if (ec) return false;

  static constexpr std::array<const char *, 4> kSaveDirectories = {
      "savedata", "save", "saves", "strsave"};
  for (const char *name : kSaveDirectories) {
    const auto target = std::filesystem::absolute(spec.save_path / name, ec);
    if (ec) return false;
    std::filesystem::create_directories(target, ec);
    if (ec) return false;
    const auto bundled = game.path / name;
    if (std::filesystem::is_directory(bundled, ec)) {
      std::filesystem::copy(
          bundled, target,
          std::filesystem::copy_options::recursive |
              std::filesystem::copy_options::skip_existing,
          ec);
      if (ec) return false;
    }
    ec.clear();
    if (!EnsureProjectViewLink(target, view / name, true, ec)) return false;
  }

  for (const auto &entry : std::filesystem::directory_iterator(game.path, ec)) {
    if (ec) return false;
    if (IsLegacySaveDirectory(entry.path())) continue;
    const auto source = std::filesystem::absolute(entry.path(), ec);
    if (ec) return false;
    const bool directory = entry.is_directory(ec);
    if (ec || !EnsureProjectViewLink(source, view / entry.path().filename(),
                                     directory, ec))
      return false;
  }

  std::filesystem::path relative = spec.entry_point.lexically_relative(game.path);
  if (relative.empty() || relative == ".")
    spec.entry_point = view;
  else if (*relative.begin() != "..")
    spec.entry_point = view / relative;
  spec.working_directory = view;
  spec.environment["ROCGALGAME_KRKR_PROJECT_VIEW"] = view.u8string();
  return true;
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
                                  ? KrkrRuntime::Krkr2
                                  : game.overrides.krkr_runtime;
  spec.environment["ROCGALGAME_KRKR_RUNTIME"] = KrkrRuntimeName(runtime);
  spec.environment["ROCGALGAME_KRKR_SAVE_PATH"] = spec.save_path.u8string();
  if (!game.overrides.compat_flags.empty()) {
    spec.environment["ROCGALGAME_KRKR_COMPAT_FLAGS"] =
        game.overrides.compat_flags;
  }
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
#ifndef _WIN32
  if (runtime == KrkrRuntime::Krkr2 &&
      !PrepareKrkr2ProjectView(spec, game, ec)) {
    return CoreSpecResult{LaunchStatus::InvalidGame, std::move(spec),
                          "failed to prepare writable KRKR2 project view: " +
                              ec.message()};
  }
#endif
  const std::filesystem::path font = PreferredGameFont(config, game);
  if (runtime == KrkrRuntime::Krkr2) {
    // krkr2's Linux host consumes the project path as argv[1].
    spec.arguments = {spec.executable.u8string(), spec.entry_point.u8string()};
#ifndef _WIN32
    const std::string requested_backend = ToLowerAscii(
        InheritedOrDefault("ROCGALGAME_KRKR_DISPLAY_BACKEND", "wayland"));
    const std::string display_backend =
        requested_backend == "xwayland" ? "xwayland" :
        requested_backend == "x11" ? "x11" : "wayland";
    spec.environment["ROCGALGAME_KRKR_DISPLAY_BACKEND"] = display_backend;
    spec.environment["ROCGALGAME_KRKR_XWAYLAND"] =
        display_backend == "xwayland" ? "1" : "0";
    spec.environment["ROCGALGAME_KRKR_XWAYLAND_WIDTH"] =
        InheritedOrDefault("ROCGALGAME_KRKR_XWAYLAND_WIDTH",
                           std::to_string(config.screen_w));
    spec.environment["ROCGALGAME_KRKR_XWAYLAND_HEIGHT"] =
        InheritedOrDefault("ROCGALGAME_KRKR_XWAYLAND_HEIGHT",
                           std::to_string(config.screen_h));
    if (display_backend == "xwayland") {
      spec.environment["ROCGALGAME_KRKR_XWAYLAND_RENDERER"] =
          InheritedOrDefault("ROCGALGAME_KRKR_XWAYLAND_RENDERER", "hardware");
      spec.environment["ROCGALGAME_KRKR_XWAYLAND_SHM"] =
          InheritedOrDefault("ROCGALGAME_KRKR_XWAYLAND_SHM", "1");
      spec.environment["DISPLAY"] = ":2";
      spec.environment["GDK_BACKEND"] = "x11";
      spec.environment["SDL_VIDEODRIVER"] = "x11";
    } else if (display_backend == "wayland") {
      spec.environment["XDG_RUNTIME_DIR"] =
          InheritedOrDefault("XDG_RUNTIME_DIR", "/run/0-runtime-dir");
      spec.environment["WAYLAND_DISPLAY"] =
          InheritedOrDefault("WAYLAND_DISPLAY", "wayland-1");
      spec.environment["SWAYSOCK"] = InheritedOrDefault(
          "SWAYSOCK", "/run/0-runtime-dir/sway-ipc.0.sock");
      spec.environment["GDK_BACKEND"] = "wayland";
      spec.environment["SDL_VIDEODRIVER"] = "wayland";
      spec.environment["MALI_WAYLAND_DMABUF_PROTOCOL"] =
          InheritedOrDefault("MALI_WAYLAND_DMABUF_PROTOCOL", "1");
      spec.environment["MALI_PLATFORM_CONFIG"] = InheritedOrDefault(
          "MALI_PLATFORM_CONFIG",
          (config.root / "mali_platform.config").u8string());
    } else {
      spec.environment["DISPLAY"] =
          InheritedOrDefault("DISPLAY", ":0");
      spec.environment["GDK_BACKEND"] = "x11";
      spec.environment["SDL_VIDEODRIVER"] = "x11";
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

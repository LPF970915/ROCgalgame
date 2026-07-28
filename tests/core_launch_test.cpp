#include "core_process_runner.h"
#include "game_core_registry.h"
#include "game_launch_service.h"
#include "game_scanner.h"
#include "game_library.h"
#include "krkr_core_adapter.h"
#include "ons_core_adapter.h"

#include <algorithm>
#include <array>
#include <cassert>
#include <chrono>
#include <cstdlib>
#include <filesystem>
#include <fstream>
#include <iostream>
#include <iterator>

namespace {
void WriteXp3Archive(const std::filesystem::path &path) {
  static constexpr std::array<unsigned char, 11> kMagic = {
      'X', 'P', '3', '\r', '\n', ' ', '\n', 0x1a, 0x8b, 0x67, 0x01};
  std::ofstream out(path, std::ios::binary);
  out.write(reinterpret_cast<const char *>(kMagic.data()),
            static_cast<std::streamsize>(kMagic.size()));
  out << "test";
}
}  // namespace

int main() {
  namespace fs = std::filesystem;
  const fs::path root = fs::temp_directory_path() / "rocgalgame_core_launch_test";
  std::error_code ec;
  fs::remove_all(root, ec);
  fs::create_directories(root / "games/krkr/directory_game");
  fs::create_directories(root / "games/krkr/archive_game");
  fs::create_directories(root / "games/krkr/native_game");
  fs::create_directories(root / "games/renamed_archive_game");
  fs::create_directories(root / "games/custom_archive_game");
  fs::create_directories(root / "games/ambiguous_archive_game");
  fs::create_directories(root / "games/krkr/fake_archive_game");
  const fs::path flat_krkr = root / "games" / fs::u8path(u8"千恋万花");
  fs::create_directories(flat_krkr);
  fs::create_directories(root / "games/flat_ons");
  std::ofstream(root / "games/krkr/directory_game/startup.tjs") << "Debug.message('test');";
  std::ofstream(root / "games/krkr/archive_game/custom.xp3") << "test";
  std::ofstream(flat_krkr / "data.xp3") << "test";
  std::ofstream(root / "games/flat_ons/0.txt") << "test";
  std::ofstream(root / "games/krkr/archive_game/game.ini")
      << "entry=custom.xp3\nruntime=krkrsdl2\nframe_limit=30\ndraw_threads=2\ngraphic_cache_mb=128\n";
  std::ofstream(root / "games/krkr/native_game/startup.tjs") << "Debug.message('test');";
  std::ofstream(root / "games/krkr/native_game/data.xp3") << "test";
  std::ofstream(root / "games/krkr/native_game/game.ini") << "runtime=krkr2\n";
  WriteXp3Archive(root / "games/renamed_archive_game/data.bin");
  WriteXp3Archive(root / "games/renamed_archive_game/patch.xp3");
  WriteXp3Archive(root / "games/custom_archive_game/project.dat");
  WriteXp3Archive(root / "games/ambiguous_archive_game/first.bin");
  WriteXp3Archive(root / "games/ambiguous_archive_game/second.dat");
  std::ofstream(root / "games/krkr/fake_archive_game/data.bin") << "not an XP3 archive";

  const auto games = ScanGameLibrary(root, "games", "covers", "saves");
  assert(games.size() == 9);
  AppConfig config; config.root = root;
  OnsCoreAdapter ons_adapter;
  KrkrCoreAdapter krkr_adapter;
  GameCoreRegistry registry;
  registry.Register(&ons_adapter);
  registry.Register(&krkr_adapter);
  CoreProcessRunner runner;
  GameLaunchService launch_service(registry, runner);
  bool saw_directory = false;
  bool saw_archive = false;
  bool saw_flat_krkr = false;
  bool saw_flat_ons = false;
  bool saw_native = false;
  bool saw_renamed_archive = false;
  bool saw_custom_archive = false;
  bool saw_ambiguous_archive = false;
  bool saw_fake_archive = false;
  for (const auto &game : games) {
    if (game.path.filename() == "flat_ons") {
      saw_flat_ons = true;
      assert(game.core == CoreKind::Ons);
      const CoreSpecResult built = launch_service.BuildSpec(config, game);
      assert(built.Ok());
      assert(built.spec.arguments.end() !=
             std::find(built.spec.arguments.begin(), built.spec.arguments.end(), "--enc:gbk"));
      assert(built.spec.arguments.end() !=
             std::find(built.spec.arguments.begin(), built.spec.arguments.end(), "--fullscreen"));
      assert(built.spec.environment.at("ROCGALGAME_FILTER") == "reflection");
      const auto default_sharpness = std::find(built.spec.arguments.begin(),
                                               built.spec.arguments.end(), "--sharpness");
      assert(default_sharpness != built.spec.arguments.end());
      assert(std::next(default_sharpness) != built.spec.arguments.end());
      assert(*std::next(default_sharpness) == "0");
      config.default_filter = "scanline";
      const CoreSpecResult filtered = launch_service.BuildSpec(config, game);
      assert(filtered.Ok());
      const auto sharpness = std::find(filtered.spec.arguments.begin(),
                                       filtered.spec.arguments.end(), "--sharpness");
      assert(sharpness != filtered.spec.arguments.end());
      assert(std::next(sharpness) != filtered.spec.arguments.end());
      assert(*std::next(sharpness) == "0");
      config.default_filter = "clean";
      const CoreSpecResult native = launch_service.BuildSpec(config, game);
      assert(native.Ok());
      assert(native.spec.arguments.end() ==
             std::find(native.spec.arguments.begin(), native.spec.arguments.end(), "--sharpness"));
      continue;
    }

    const CoreSpecResult built = launch_service.BuildSpec(config, game);
    assert(built.Ok());
    const CoreLaunchSpec &spec = built.spec;
    assert(spec.environment.at("ROCGALGAME_KRKR_VIRTUAL_MOUSE") == "1");
    assert(spec.environment.at("ROCGALGAME_KRKR_SWAP_AB") == "1");
    assert(spec.environment.at("ROCGALGAME_MOUSE_SPEED") == "1080");
    assert(spec.environment.at("ROCGALGAME_MOUSE_ACCEL") == "1.000000");
    assert(spec.environment.count("ROCGALGAME_KRKR_CONTINUOUS_PRESENT") == 0);
    assert(spec.environment.count("AETHERKIRI_MOTION_DEBUG") == 0);
    if (game.path.filename() == "native_game") {
      saw_native = true;
      assert(game.overrides.krkr_runtime == KrkrRuntime::Krkr2);
      assert(spec.executable.filename() == "krkr2");
      assert(spec.arguments.size() == 2);
      assert(fs::u8path(spec.arguments[1]).filename() == "data.xp3");
      assert(spec.environment.at("ROCGALGAME_KRKR_RUNTIME") == "krkr2");
      assert(spec.environment.at("ROCGALGAME_KRKR_DISPLAY_BACKEND") ==
             "wayland");
      assert(spec.environment.at("ROCGALGAME_KRKR_XWAYLAND") == "0");
      assert(spec.environment.at("ROCGALGAME_KRKR_XWAYLAND_WIDTH") == "1600");
      assert(spec.environment.at("ROCGALGAME_KRKR_XWAYLAND_HEIGHT") == "1440");
      assert(spec.environment.at("ROCGALGAME_INPUT_PROFILE") ==
             "gkd350h-ultra");
      assert(spec.environment.at("XDG_RUNTIME_DIR") == "/run/0-runtime-dir");
      assert(spec.environment.at("WAYLAND_DISPLAY") == "wayland-1");
      assert(spec.environment.at("SWAYSOCK") ==
             "/run/0-runtime-dir/sway-ipc.0.sock");
      assert(spec.environment.at("GDK_BACKEND") == "wayland");
      assert(spec.environment.at("SDL_VIDEODRIVER") == "wayland");
      assert(spec.environment.count("DISPLAY") == 0);
      assert(spec.environment.at("LD_LIBRARY_PATH").rfind("/usr/lib/mali:", 0) ==
             0);
      assert(spec.environment.at("LD_LIBRARY_PATH").find("lib_krkr2") !=
             std::string::npos);
#if defined(__linux__)
      setenv("ROCGALGAME_KRKR_DISPLAY_BACKEND", "xwayland", 1);
      const CoreSpecResult xwayland = krkr_adapter.BuildSpec(config, game);
      unsetenv("ROCGALGAME_KRKR_DISPLAY_BACKEND");
      assert(xwayland.Ok());
      assert(xwayland.spec.environment.at("ROCGALGAME_KRKR_DISPLAY_BACKEND") ==
             "xwayland");
      assert(xwayland.spec.environment.at("ROCGALGAME_KRKR_XWAYLAND") == "1");
      assert(xwayland.spec.environment.at("DISPLAY") == ":2");
      assert(xwayland.spec.environment.at("GDK_BACKEND") == "x11");
      assert(xwayland.spec.environment.at("SDL_VIDEODRIVER") == "x11");
      assert(xwayland.spec.environment.at("LD_LIBRARY_PATH").rfind(
                 (root / "cores/krkr/lib_krkr2").u8string(), 0) == 0);
#endif
      AppConfig legacy = config;
      legacy.mouse_speed = 720;
      legacy.mouse_accel = 1.6f;
      const CoreSpecResult migrated = krkr_adapter.BuildSpec(legacy, game);
      assert(migrated.Ok());
      assert(migrated.spec.environment.at("ROCGALGAME_MOUSE_SPEED") == "1080");
      assert(migrated.spec.environment.at("ROCGALGAME_MOUSE_ACCEL") ==
             "1.000000");
    } else if (game.path.filename() == "renamed_archive_game") {
      saw_renamed_archive = true;
      assert(game.entry_point.filename() == "data.bin");
      assert(game.overrides.krkr_runtime == KrkrRuntime::Krkr2);
      assert(spec.executable.filename() == "krkr2");
      assert(spec.arguments.size() == 2);
      assert(fs::u8path(spec.arguments[1]).filename() == "data.bin");
    } else if (game.path.filename() == "custom_archive_game") {
      saw_custom_archive = true;
      assert(game.entry_point.filename() == "project.dat");
      assert(game.overrides.krkr_runtime == KrkrRuntime::Krkr2);
      assert(spec.executable.filename() == "krkr2");
    } else if (game.path.filename() == "ambiguous_archive_game") {
      saw_ambiguous_archive = true;
      assert(game.entry_point == game.path);
      assert(game.overrides.krkr_runtime == KrkrRuntime::Auto);
      assert(spec.executable.filename() == "krkrsdl2");
    } else if (game.path.filename() == "fake_archive_game") {
      saw_fake_archive = true;
      assert(game.entry_point == game.path);
      assert(game.overrides.krkr_runtime == KrkrRuntime::Auto);
      assert(spec.executable.filename() == "krkrsdl2");
    } else if (game.path.filename() == "directory_game") {
      saw_directory = true;
      assert(game.entry_point == game.path);
    } else if (game.path.filename() == "archive_game") {
      saw_archive = true;
      assert(game.entry_point.filename() == "custom.xp3");
      assert(spec.arguments.end() != std::find(spec.arguments.begin(), spec.arguments.end(), "-contfreq=30"));
      assert(spec.arguments.end() != std::find(spec.arguments.begin(), spec.arguments.end(), "-drawthread=2"));
      assert(spec.arguments.end() != std::find(spec.arguments.begin(), spec.arguments.end(), "-gclim=128"));
      const auto font_arg = std::find_if(spec.arguments.begin(), spec.arguments.end(), [](const std::string &arg) {
        return arg.rfind("-deffont=", 0) == 0;
      });
      assert(font_arg != spec.arguments.end());
      assert(fs::u8path(font_arg->substr(9)).filename() == "ui_font_02.ttf");
      AppConfig legacy = config;
      legacy.mouse_speed = 720;
      legacy.mouse_accel = 1.6f;
      const CoreSpecResult unchanged = krkr_adapter.BuildSpec(legacy, game);
      assert(unchanged.Ok());
      assert(unchanged.spec.environment.at("ROCGALGAME_MOUSE_SPEED") == "720");
      assert(unchanged.spec.environment.at("ROCGALGAME_MOUSE_ACCEL") ==
             "1.600000");
    } else if (game.path.filename() == fs::u8path(u8"千恋万花")) {
      saw_flat_krkr = true;
      assert(game.core == CoreKind::Krkr);
      assert(game.entry_point == game.path);
    }
  }
  assert(saw_directory && saw_archive && saw_flat_krkr && saw_flat_ons && saw_native &&
         saw_renamed_archive && saw_custom_archive && saw_ambiguous_archive &&
         saw_fake_archive);
#if defined(__linux__)
  CoreLaunchSpec supervised;
  supervised.executable = "/bin/sh";
  supervised.working_directory = root;
  supervised.save_path = root / "saves/supervised";
  supervised.log_path = root / "supervised.log";
  supervised.arguments = {"/bin/sh", "-c", "sleep 30"};
  int polls = 0;
  const auto started = std::chrono::steady_clock::now();
  const LaunchResult stopped = runner.Run(supervised, [&polls]() {
    return ++polls >= 4;
  });
  const auto elapsed = std::chrono::steady_clock::now() - started;
  assert(stopped.status == LaunchStatus::NormalExit);
  assert(stopped.exit_code == 0);
  assert(stopped.detail == "user requested exit");
  assert(polls >= 4);
  assert(elapsed < std::chrono::seconds(3));
#endif
  fs::remove_all(root, ec);
  std::cout << "core launch tests passed\n";
}

#include "core_process_runner.h"
#include "game_core_registry.h"
#include "game_launch_service.h"
#include "game_scanner.h"
#include "game_library.h"
#include "krkr_core_adapter.h"
#include "ons_core_adapter.h"

#include <algorithm>
#include <cassert>
#include <filesystem>
#include <fstream>
#include <iostream>
#include <iterator>

int main() {
  namespace fs = std::filesystem;
  const fs::path root = fs::temp_directory_path() / "rocgalgame_core_launch_test";
  std::error_code ec;
  fs::remove_all(root, ec);
  fs::create_directories(root / "games/krkr/directory_game");
  fs::create_directories(root / "games/krkr/archive_game");
  const fs::path flat_krkr = root / "games" / fs::u8path(u8"千恋万花");
  fs::create_directories(flat_krkr);
  fs::create_directories(root / "games/flat_ons");
  std::ofstream(root / "games/krkr/directory_game/startup.tjs") << "Debug.message('test');";
  std::ofstream(root / "games/krkr/archive_game/custom.xp3") << "test";
  std::ofstream(flat_krkr / "data.xp3") << "test";
  std::ofstream(root / "games/flat_ons/0.txt") << "test";
  std::ofstream(root / "games/krkr/archive_game/game.ini")
      << "entry=custom.xp3\nframe_limit=30\ndraw_threads=2\ngraphic_cache_mb=128\n";

  const auto games = ScanGameLibrary(root, "games", "covers", "saves");
  assert(games.size() == 4);
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
      assert(built.spec.arguments.end() ==
             std::find(built.spec.arguments.begin(), built.spec.arguments.end(), "--sharpness"));
      config.default_filter = "scanline";
      const CoreSpecResult filtered = launch_service.BuildSpec(config, game);
      assert(filtered.Ok());
      const auto sharpness = std::find(filtered.spec.arguments.begin(),
                                       filtered.spec.arguments.end(), "--sharpness");
      assert(sharpness != filtered.spec.arguments.end());
      assert(std::next(sharpness) != filtered.spec.arguments.end());
      assert(*std::next(sharpness) == "0");
      config.default_filter = "clean";
      continue;
    }

    const CoreSpecResult built = launch_service.BuildSpec(config, game);
    assert(built.Ok());
    const CoreLaunchSpec &spec = built.spec;
    assert(spec.environment.at("ROCGALGAME_KRKR_VIRTUAL_MOUSE") == "1");
    assert(spec.environment.at("ROCGALGAME_KRKR_SWAP_AB") == "1");
    assert(spec.environment.at("ROCGALGAME_KRKR_CONTINUOUS_PRESENT") == "1");
    assert(spec.environment.at("AETHERKIRI_MOTION_DEBUG") == "1");
    if (game.path.filename() == "directory_game") {
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
    } else if (game.path.filename() == fs::u8path(u8"千恋万花")) {
      saw_flat_krkr = true;
      assert(game.core == CoreKind::Krkr);
      assert(game.entry_point == game.path);
    }
  }
  assert(saw_directory && saw_archive && saw_flat_krkr && saw_flat_ons);
  fs::remove_all(root, ec);
  std::cout << "core launch tests passed\n";
}

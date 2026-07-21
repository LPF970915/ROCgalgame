#include "app_stores.h"
#include "game_core_registry.h"
#include "game_launch_service.h"
#include "game_library_service.h"
#include "game_settings_runtime.h"
#include "shelf_runtime.h"

#include <cassert>
#include <filesystem>
#include <fstream>
#include <iostream>

namespace {
class TyranoTestAdapter final : public IGameCoreAdapter {
public:
  CoreKind Kind() const override { return CoreKind::Tyrano; }

  CoreSpecResult BuildSpec(const AppConfig &config,
                           const GameEntry &game) const override {
    if (game.core != Kind()) {
      return CoreSpecResult{LaunchStatus::Unsupported, {}, "core mismatch"};
    }
    CoreLaunchSpec spec;
    spec.core = game.core;
    spec.executable = config.root / "cores" / "tyrano" / "runner";
    spec.working_directory = game.path;
    spec.arguments = {spec.executable.u8string(), game.path.u8string()};
    return CoreSpecResult{LaunchStatus::NormalExit, std::move(spec), {}};
  }
};

class FakeProcessRunner final : public ICoreProcessRunner {
public:
  LaunchResult Run(const CoreLaunchSpec &spec) const override {
    ++calls;
    last_core = spec.core;
    return next_result;
  }

  mutable int calls = 0;
  mutable CoreKind last_core = CoreKind::Unknown;
  LaunchResult next_result{LaunchStatus::CoreError, 9, 0, {}, "test"};
};

void CreateOnsGame(const std::filesystem::path &directory) {
  std::filesystem::create_directories(directory);
  std::ofstream(directory / "0.txt") << "test";
}
}  // namespace

int main() {
  namespace fs = std::filesystem;
  const GameSettingsState defaults;
  assert(defaults.aspect == GameAspectMode::Contain);
  assert(defaults.filter == GameFilterMode::Reflection);

  const fs::path root = fs::temp_directory_path() / "rocgalgame_game_domain_test";
  std::error_code ec;
  fs::remove_all(root, ec);
  fs::create_directories(root);

  const fs::path config_path = root / "native_config.ini";
  std::ofstream(config_path)
      << "games_root=games\n"
      << "covers_root=covers\n"
      << "saves_root=saves\n"
      << "default_aspect=fit-width\n"
      << "default_filter=clean\n"
      << "virtual_mouse=1\n"
      << "mouse_speed=720\n"
      << "mouse_accel=1.60\n";

  ConfigStore store;
  store.LoadFromPath(config_path);
  GameSettingsState settings = MakeGameSettingsState(store.Get());
  assert(settings.aspect == GameAspectMode::Contain);
  assert(settings.virtual_mouse);
  const GameSettingsCallbacks callbacks =
      MakeGameSettingsCallbacks(store, []() { return 123u; });
  assert(callbacks.set_aspect(GameAspectMode::FillHeight, settings));
  assert(callbacks.set_filter(GameFilterMode::Reflection, settings));
  assert(callbacks.set_virtual_mouse(false, settings));
  assert(callbacks.set_mouse_speed(960, settings));
  assert(callbacks.set_mouse_acceleration(2.0f, settings));
  assert(store.IsDirty());
  assert(store.Save());

  ConfigStore reloaded;
  reloaded.LoadFromPath(config_path);
  const GameSettingsSnapshot snapshot = CaptureGameSettings(reloaded.Get());
  assert(snapshot.aspect == "fill-height");
  assert(snapshot.filter == "reflection");
  assert(!snapshot.virtual_mouse);
  assert(snapshot.mouse_speed == 960);
  assert(snapshot.mouse_acceleration == 2.0f);
  assert(LocalizedGameAspectLabel(9, GameAspectMode::Contain) == "Contain");
  assert(ParseGameFilterMode("crt-soft") == GameFilterMode::AntiAlias);
  assert(ParseGameFilterMode("mask") == GameFilterMode::DotMatrix);

  for (int i = 0; i < 5; ++i) {
    CreateOnsGame(root / "games" / ("ons_" + std::to_string(i)));
  }
  fs::create_directories(root / "games" / "krkr_0");
  std::ofstream(root / "games" / "krkr_0" / "data.xp3") << "test";

  GameLibraryService library;
  library.Configure(reloaded.Get());
  library.LoadPersistentState();
  library.Scan();
  assert(library.Games().size() == 6);
  assert(library.VisibleIndices(0).size() == 5);
  assert(library.VisibleIndices(1).size() == 1);

  const GameEntry &first = library.Games().front();
  const GameEntry &second = library.Games()[1];
  assert(library.AddFavorite(first));
  assert(!library.AddFavorite(first));
  assert(library.VisibleIndices(2).size() == 1);
  library.PushHistory(first);
  library.PushHistory(second);
  const std::vector<int> history = library.VisibleIndices(3);
  assert(history.size() == 2);
  assert(library.GameAtVisibleIndex(history, 0)->path == second.path);

  ShelfRuntimeState shelf;
  RebuildShelfItems(shelf, library, 4);
  assert(shelf.visible_games.size() == 5);
  for (int i = 0; i < 4; ++i) MoveShelfFocus(shelf, 1, 0, 4);
  assert(shelf.page == 1);
  assert(shelf.focus_index == 4);
  MoveShelfFocus(shelf, -1, 0, 4);
  assert(shelf.page == 0);
  assert(shelf.focus_index == 3);
  MoveShelfFocus(shelf, 0, 1, 4);
  assert(shelf.page == 1);
  assert(shelf.focus_index == 4);
  MoveShelfFocus(shelf, 0, -1, 4);
  assert(shelf.page == 0);
  assert(shelf.focus_index == 0);
  ChangeShelfCategory(shelf, library, 1, 4);
  assert(shelf.category_index == 1);
  assert(shelf.visible_games.size() == 1);

  TyranoTestAdapter tyrano;
  GameCoreRegistry registry;
  registry.Register(&tyrano);
  assert(registry.Find(CoreKind::Tyrano) == &tyrano);
  FakeProcessRunner runner;
  GameLaunchService launch_service(registry, runner);
  GameEntry tyrano_game;
  tyrano_game.core = CoreKind::Tyrano;
  tyrano_game.path = root / "games" / "tyrano_0";
  const LaunchResult launched = launch_service.Launch(reloaded.Get(), tyrano_game);
  assert(launched.status == LaunchStatus::CoreError);
  assert(launched.exit_code == 9);
  assert(runner.calls == 1);
  assert(runner.last_core == CoreKind::Tyrano);
  assert(DescribeLaunchResult(launched, 2) == "Core exited with code 9");

  fs::remove_all(root, ec);
  std::cout << "game domain runtime tests passed\n";
  return 0;
}

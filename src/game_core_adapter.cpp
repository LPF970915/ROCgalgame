#include "game_core_adapter.h"

#include <chrono>
#include <iomanip>
#include <sstream>
#include <system_error>

namespace {
std::string Timestamp() {
  const auto now = std::chrono::system_clock::now();
  const std::time_t value = std::chrono::system_clock::to_time_t(now);
  std::tm local{};
#ifdef _WIN32
  localtime_s(&local, &value);
#else
  localtime_r(&value, &local);
#endif
  std::ostringstream out;
  out << std::put_time(&local, "%Y%m%d_%H%M%S");
  return out.str();
}
}  // namespace

EffectiveGameSettings ResolveEffectiveGameSettings(const AppConfig &config,
                                                   const GameEntry &game) {
  const GameSettingsSnapshot defaults = CaptureGameSettings(config);
  return EffectiveGameSettings{
      game.overrides.aspect.empty() ? defaults.aspect : game.overrides.aspect,
      game.overrides.filter.empty() ? defaults.filter : game.overrides.filter,
      game.overrides.has_virtual_mouse ? game.overrides.virtual_mouse
                                       : defaults.virtual_mouse,
      game.overrides.mouse_speed > 0 ? game.overrides.mouse_speed
                                     : defaults.mouse_speed,
      game.overrides.mouse_accel > 0.0f ? game.overrides.mouse_accel
                                        : defaults.mouse_acceleration,
  };
}

CoreLaunchSpec MakeBaseCoreLaunchSpec(const AppConfig &config,
                                      const GameEntry &game,
                                      const EffectiveGameSettings &settings) {
  CoreLaunchSpec spec;
  spec.core = game.core;
  spec.working_directory = game.path;
  spec.entry_point = game.entry_point;
  spec.save_path = game.save_path;
  spec.log_path = config.root / "logs" / CoreKindName(game.core) /
                  (game.path.filename().u8string() + "_" + Timestamp() + ".log");
  spec.environment["SDL_NOMOUSE"] = "0";
  spec.environment["ROCGALGAME_VIRTUAL_MOUSE"] = settings.virtual_mouse ? "1" : "0";
  spec.environment["ROCGALGAME_MOUSE_SPEED"] = std::to_string(settings.mouse_speed);
  spec.environment["ROCGALGAME_MOUSE_ACCEL"] = std::to_string(settings.mouse_acceleration);
  spec.environment["ROCGALGAME_ASPECT"] = settings.aspect;
  spec.environment["ROCGALGAME_FILTER"] = settings.filter;
  return spec;
}

std::filesystem::path PreferredGameFont(const AppConfig &config,
                                        const GameEntry &game) {
  std::error_code ec;
  for (const auto &font : {game.path / "default.ttf",
                           config.root / "fonts" / "ui_font_02.ttf",
                           config.root / "fonts" / "ui_font.ttf"}) {
    if (std::filesystem::is_regular_file(font, ec)) return font;
    ec.clear();
  }
  return config.root / "fonts" / "ui_font_02.ttf";
}

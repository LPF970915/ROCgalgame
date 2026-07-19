#include "ons_core_adapter.h"

#include <functional>
#include <system_error>

CoreSpecResult OnsCoreAdapter::BuildSpec(const AppConfig &config,
                                         const GameEntry &game) const {
  if (game.core != Kind()) return CoreSpecResult{LaunchStatus::Unsupported, {}, "core mismatch"};
  std::error_code ec;
  if (!std::filesystem::is_directory(game.path, ec)) {
    return CoreSpecResult{LaunchStatus::InvalidGame, {}, "game directory missing"};
  }
  const EffectiveGameSettings settings = ResolveEffectiveGameSettings(config, game);
  CoreLaunchSpec spec = MakeBaseCoreLaunchSpec(config, game, settings);
#ifdef _WIN32
  spec.executable = config.root / "cores" / "ons" / "onsyuri.exe";
  spec.save_path = config.root / "Windows" / "saves" /
                   std::to_string(std::hash<std::wstring>{}(game.path.wstring()));
#else
  spec.executable = config.root / "cores" / "ons" / "onsyuri";
#endif
  const std::string encoding = game.overrides.encoding.empty()
                                   ? "gbk" : game.overrides.encoding;
  std::string encoding_arg = "--enc:gbk";
  if (encoding == "sjis" || encoding == "shift-jis" || encoding == "shift_jis") {
    encoding_arg = "--enc:sjis";
  } else if (encoding == "utf8" || encoding == "utf-8") {
    encoding_arg = "--enc:utf8";
  }
  const std::filesystem::path font = PreferredGameFont(config, game);
  spec.arguments = {spec.executable.u8string(), "--root", game.path.u8string(),
                    "--save-dir", spec.save_path.u8string(), "--font",
                    font.u8string(), encoding_arg, "--force-button-shortcut"};
  if (settings.filter != "clean") {
    spec.arguments.push_back("--sharpness");
    spec.arguments.push_back("0");
  }
  spec.arguments.push_back(settings.aspect == "stretch" ? "--fullscreen2" : "--fullscreen");
  return CoreSpecResult{LaunchStatus::NormalExit, std::move(spec), {}};
}

#include "core_launcher.h"

#include <cstdlib>
#include <fstream>
#include <functional>
#include <system_error>
#include <vector>

#if defined(__linux__)
#include <cerrno>
#include <sys/wait.h>
#include <unistd.h>
#elif defined(_WIN32)
#include <process.h>
#endif

namespace {
std::string EffectiveString(const std::string &override_value, const std::string &default_value) {
  return override_value.empty() ? default_value : override_value;
}
}  // namespace

bool WriteLaunchRequest(const AppConfig &config, const GameEntry &game) {
  std::error_code ec;
  const std::filesystem::path cache_dir = config.root / "cache";
  std::filesystem::create_directories(cache_dir, ec);
  std::filesystem::create_directories(game.save_path, ec);
  std::ofstream out(cache_dir / "launch_request.ini", std::ios::trunc);
  if (!out) return false;
  out << "core=" << CoreKindName(game.core) << "\n";
  out << "path=" << game.path.u8string() << "\n";
  out << "save=" << game.save_path.u8string() << "\n";
  out << "encoding=" << EffectiveString(game.overrides.encoding, "gbk") << "\n";
  out << "aspect=" << EffectiveString(game.overrides.aspect, config.default_aspect) << "\n";
  out << "filter=" << EffectiveString(game.overrides.filter, config.default_filter) << "\n";
  out << "virtual_mouse=" << ((game.overrides.has_virtual_mouse ? game.overrides.virtual_mouse : config.virtual_mouse) ? "1" : "0") << "\n";
  out << "mouse_speed=" << (game.overrides.mouse_speed > 0 ? game.overrides.mouse_speed : config.mouse_speed) << "\n";
  out << "mouse_accel=" << (game.overrides.mouse_accel > 0.0f ? game.overrides.mouse_accel : config.mouse_accel) << "\n";
  return true;
}

int LaunchGameAndWait(const AppConfig &config, const GameEntry &game) {
#if !defined(__linux__) && !defined(_WIN32)
  (void)config;
  (void)game;
  return -2;
#elif defined(_WIN32)
  if (game.core != CoreKind::Ons) return -2;

  std::error_code ec;
  const std::filesystem::path executable = config.root / "cores" / "ons" / "onsyuri.exe";
  if (!std::filesystem::is_regular_file(executable, ec)) return -1;
  const std::filesystem::path windows_save_path =
      config.root / "Windows" / "saves" / std::to_string(std::hash<std::wstring>{}(game.path.wstring()));
  std::filesystem::create_directories(windows_save_path, ec);

  const std::string encoding = EffectiveString(game.overrides.encoding, "gbk");
  const std::string aspect = EffectiveString(game.overrides.aspect, config.default_aspect);
  const std::string filter = EffectiveString(game.overrides.filter, config.default_filter);
  const bool virtual_mouse = game.overrides.has_virtual_mouse ? game.overrides.virtual_mouse : config.virtual_mouse;
  const int mouse_speed = game.overrides.mouse_speed > 0 ? game.overrides.mouse_speed : config.mouse_speed;
  const float mouse_accel = game.overrides.mouse_accel > 0.0f ? game.overrides.mouse_accel : config.mouse_accel;
  const std::filesystem::path game_font = game.path / "default.ttf";
  const std::filesystem::path font = std::filesystem::is_regular_file(game_font, ec)
                                         ? std::filesystem::path(L"default.ttf")
                                         : config.root / "fonts" / "ui_font.ttf";

  std::wstring encoding_arg = L"--enc:gbk";
  if (encoding == "sjis" || encoding == "shift-jis" || encoding == "shift_jis") encoding_arg = L"--enc:sjis";
  else if (encoding == "utf8" || encoding == "utf-8") encoding_arg = L"--enc:utf8";

  _wputenv_s(L"SDL_NOMOUSE", L"0");
  _wputenv_s(L"ROCGALGAME_VIRTUAL_MOUSE", virtual_mouse ? L"1" : L"0");
  _wputenv_s(L"ROCGALGAME_MOUSE_SPEED", std::to_wstring(mouse_speed).c_str());
  _wputenv_s(L"ROCGALGAME_MOUSE_ACCEL", std::to_wstring(mouse_accel).c_str());
  _wputenv_s(L"ROCGALGAME_ASPECT", std::wstring(aspect.begin(), aspect.end()).c_str());
  _wputenv_s(L"ROCGALGAME_FILTER", std::wstring(filter.begin(), filter.end()).c_str());

  std::vector<std::wstring> args = {
      executable.wstring(),
      L"--root", L".",
      L"--save-dir", windows_save_path.wstring(),
      L"--font", font.wstring(),
      encoding_arg,
      L"--force-button-shortcut",
  };
  const char *windowed = std::getenv("ROCGALGAME_WINDOWED");
  if (!windowed || std::string(windowed) != "1")
    args.push_back(aspect == "stretch" ? L"--fullscreen2" : L"--fullscreen");
  std::vector<const wchar_t *> argv;
  argv.reserve(args.size() + 1);
  for (const std::wstring &arg : args) argv.push_back(arg.c_str());
  argv.push_back(nullptr);

  const std::filesystem::path previous_path = std::filesystem::current_path(ec);
  std::filesystem::current_path(game.path, ec);
  if (ec) return -1;
  const intptr_t result = _wspawnv(_P_WAIT, executable.c_str(), argv.data());
  std::filesystem::current_path(previous_path, ec);
  return result < 0 ? -1 : static_cast<int>(result);
#else
  if (game.core != CoreKind::Ons) return -2;

  std::error_code ec;
  std::filesystem::create_directories(game.save_path, ec);
  const std::filesystem::path executable = config.root / "cores" / "ons" / "onsyuri";
  if (!std::filesystem::is_regular_file(executable, ec)) return -1;

  const std::string encoding = EffectiveString(game.overrides.encoding, "gbk");
  const std::string aspect = EffectiveString(game.overrides.aspect, config.default_aspect);
  const std::string filter = EffectiveString(game.overrides.filter, config.default_filter);
  const bool virtual_mouse = game.overrides.has_virtual_mouse ? game.overrides.virtual_mouse : config.virtual_mouse;
  const int mouse_speed = game.overrides.mouse_speed > 0 ? game.overrides.mouse_speed : config.mouse_speed;
  const float mouse_accel = game.overrides.mouse_accel > 0.0f ? game.overrides.mouse_accel : config.mouse_accel;
  const std::filesystem::path game_font = game.path / "default.ttf";
  const std::filesystem::path font = std::filesystem::is_regular_file(game_font, ec)
                                         ? game_font
                                         : config.root / "fonts" / "ui_font.ttf";

  std::string encoding_arg = "--enc:gbk";
  if (encoding == "sjis" || encoding == "shift-jis" || encoding == "shift_jis") encoding_arg = "--enc:sjis";
  else if (encoding == "utf8" || encoding == "utf-8") encoding_arg = "--enc:utf8";

  std::vector<std::string> args = {
      executable.string(),
      "--root", game.path.string(),
      "--save-dir", game.save_path.string(),
      "--font", font.string(),
      encoding_arg,
      "--force-button-shortcut",
      aspect == "stretch" ? "--fullscreen2" : "--fullscreen",
  };
  std::vector<char *> argv;
  argv.reserve(args.size() + 1);
  for (std::string &arg : args) argv.push_back(const_cast<char *>(arg.c_str()));
  argv.push_back(nullptr);

  const pid_t child = fork();
  if (child < 0) return -1;
  if (child == 0) {
    setenv("SDL_NOMOUSE", "0", 1);
    setenv("ROCGALGAME_VIRTUAL_MOUSE", virtual_mouse ? "1" : "0", 1);
    setenv("ROCGALGAME_MOUSE_SPEED", std::to_string(mouse_speed).c_str(), 1);
    setenv("ROCGALGAME_MOUSE_ACCEL", std::to_string(mouse_accel).c_str(), 1);
    setenv("ROCGALGAME_ASPECT", aspect.c_str(), 1);
    setenv("ROCGALGAME_FILTER", filter.c_str(), 1);
    if (chdir(game.path.c_str()) != 0) _exit(126);
    execv(executable.c_str(), argv.data());
    _exit(127);
  }

  int status = 0;
  while (waitpid(child, &status, 0) < 0) {
    if (errno != EINTR) return -1;
  }
  if (WIFEXITED(status)) return WEXITSTATUS(status);
  if (WIFSIGNALED(status)) return 128 + WTERMSIG(status);
  return -1;
#endif
}

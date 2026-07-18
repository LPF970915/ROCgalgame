#include "core_launcher.h"

#include <chrono>
#include <cstdlib>
#include <fstream>
#include <functional>
#include <iomanip>
#include <sstream>
#include <system_error>

#if defined(__linux__)
#include <cerrno>
#include <fcntl.h>
#include <sys/wait.h>
#include <unistd.h>
#elif defined(_WIN32)
#define WIN32_LEAN_AND_MEAN
#include <windows.h>
#endif

namespace {
std::string EffectiveString(const std::string &override_value, const std::string &default_value) {
  return override_value.empty() ? default_value : override_value;
}

std::filesystem::path PreferredGameFont(const AppConfig &config, const GameEntry &game) {
  std::error_code ec;
  for (const auto &font : {game.path / "default.ttf", config.root / "fonts" / "ui_font_02.ttf",
                           config.root / "fonts" / "ui_font.ttf"}) {
    if (std::filesystem::is_regular_file(font, ec)) return font;
    ec.clear();
  }
  return config.root / "fonts" / "ui_font_02.ttf";
}

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

#ifdef _WIN32
std::filesystem::path ExistingExecutable(const std::vector<std::filesystem::path> &candidates) {
  std::error_code ec;
  for (const auto &candidate : candidates) {
    if (std::filesystem::is_regular_file(candidate, ec)) return candidate;
    ec.clear();
  }
  return candidates.empty() ? std::filesystem::path{} : candidates.front();
}
#endif

#ifndef _WIN32
std::string PluginSearchPath(const AppConfig &config, const GameEntry &game) {
  return (game.path / "plugin").u8string() + ":" +
         (config.root / "cores" / "krkr" / "plugin").u8string() + ":" +
         (config.root / "plugin").u8string();
}
#endif

#ifdef _WIN32
std::wstring QuoteWindowsArg(const std::wstring &arg) {
  if (arg.find_first_of(L" \t\"") == std::wstring::npos) return arg;
  std::wstring out = L"\"";
  unsigned slashes = 0;
  for (wchar_t c : arg) {
    if (c == L'\\') { ++slashes; continue; }
    if (c == L'\"') out.append(slashes * 2 + 1, L'\\');
    else out.append(slashes, L'\\');
    slashes = 0;
    out.push_back(c);
  }
  out.append(slashes * 2, L'\\');
  out.push_back(L'\"');
  return out;
}
#endif
}  // namespace

bool BuildCoreLaunchSpec(const AppConfig &config, const GameEntry &game, CoreLaunchSpec &spec) {
  spec = CoreLaunchSpec{};
  spec.core = game.core;
  spec.working_directory = game.path;
  spec.save_path = game.save_path;
  spec.log_path = config.root / "logs" / CoreKindName(game.core) /
                  (game.path.filename().u8string() + "_" + Timestamp() + ".log");

  const std::string aspect = EffectiveString(game.overrides.aspect, config.default_aspect);
  const std::string filter = EffectiveString(game.overrides.filter, config.default_filter);
  const bool virtual_mouse = game.overrides.has_virtual_mouse ? game.overrides.virtual_mouse : config.virtual_mouse;
  const int mouse_speed = game.overrides.mouse_speed > 0 ? game.overrides.mouse_speed : config.mouse_speed;
  const float mouse_accel = game.overrides.mouse_accel > 0.0f ? game.overrides.mouse_accel : config.mouse_accel;
  spec.environment["SDL_NOMOUSE"] = "0";
  spec.environment["ROCGALGAME_VIRTUAL_MOUSE"] = virtual_mouse ? "1" : "0";
  spec.environment["ROCGALGAME_MOUSE_SPEED"] = std::to_string(mouse_speed);
  spec.environment["ROCGALGAME_MOUSE_ACCEL"] = std::to_string(mouse_accel);
  spec.environment["ROCGALGAME_ASPECT"] = aspect;
  spec.environment["ROCGALGAME_FILTER"] = filter;

  std::error_code ec;
  if (!std::filesystem::is_directory(spec.working_directory, ec)) return false;

  if (game.core == CoreKind::Ons) {
#ifdef _WIN32
    spec.executable = config.root / "cores" / "ons" / "onsyuri.exe";
    spec.save_path = config.root / "Windows" / "saves" /
                     std::to_string(std::hash<std::wstring>{}(game.path.wstring()));
#else
    spec.executable = config.root / "cores" / "ons" / "onsyuri";
#endif
    const std::string encoding = EffectiveString(game.overrides.encoding, "gbk");
    std::string encoding_arg = "--enc:gbk";
    if (encoding == "sjis" || encoding == "shift-jis" || encoding == "shift_jis") encoding_arg = "--enc:sjis";
    else if (encoding == "utf8" || encoding == "utf-8") encoding_arg = "--enc:utf8";
    const auto font = PreferredGameFont(config, game);
    spec.arguments = {spec.executable.u8string(), "--root", game.path.u8string(), "--save-dir",
                      spec.save_path.u8string(), "--font", font.u8string(), encoding_arg,
                      "--force-button-shortcut", aspect == "stretch" ? "--fullscreen2" : "--fullscreen"};
    return true;
  }

  if (game.core == CoreKind::Krkr) {
#ifdef _WIN32
    spec.executable = ExistingExecutable({config.root / "cores" / "krkr" / "tvpwin64.exe",
                                          config.root / "cores" / "krkr" / "krkrsdl2.exe"});
#else
    spec.executable = config.root / "cores" / "krkr" / "krkrsdl2";
#endif
    spec.entry_point = game.entry_point.empty() ? game.path : game.entry_point;
    if (!std::filesystem::exists(spec.entry_point, ec)) return false;
    const int frame_limit = game.overrides.frame_limit > 0 ? game.overrides.frame_limit : 60;
    const std::string draw_threads = game.overrides.draw_threads.empty() ? "auto" : game.overrides.draw_threads;
    const int graphic_cache_mb = game.overrides.graphic_cache_mb > 0 ? game.overrides.graphic_cache_mb : 96;
    const auto font = PreferredGameFont(config, game);
    spec.arguments = {spec.executable.u8string(), spec.entry_point.u8string(),
                      "-datapath=" + spec.save_path.u8string(), "-contfreq=" + std::to_string(frame_limit),
                      "-drawthread=" + draw_threads, "-gclim=" + std::to_string(graphic_cache_mb),
                      "-deffont=" + font.u8string(), "-nosel"};
    // KRKRSDL2 uses core-specific names for its handheld presentation and
    // cursor paths. Keep the generic frontend setting as well for other cores.
    spec.environment["ROCGALGAME_KRKR_VIRTUAL_MOUSE"] = virtual_mouse ? "1" : "0";
    // GKD350H Ultra uses Nintendo-style physical face-button placement.
    spec.environment["ROCGALGAME_KRKR_SWAP_AB"] = "1";
    spec.environment["ROCGALGAME_KRKR_CONTINUOUS_PRESENT"] = "1";
    spec.environment["AETHERKIRI_MOTION_DEBUG"] = "1";
#ifndef _WIN32
    spec.environment["KRKRSDL2_PATH"] = PluginSearchPath(config, game);
#endif
    return true;
  }
  return false;
}

LaunchResult LaunchGameAndWait(const AppConfig &config, const GameEntry &game) {
  CoreLaunchSpec spec;
  LaunchResult result;
  if (!BuildCoreLaunchSpec(config, game, spec)) { result.status = LaunchStatus::InvalidGame; return result; }
  result.log_path = spec.log_path;
  std::error_code ec;
  std::filesystem::create_directories(spec.save_path, ec);
  std::filesystem::create_directories(spec.log_path.parent_path(), ec);
  if (!std::filesystem::is_regular_file(spec.executable, ec)) { result.status = LaunchStatus::MissingCore; return result; }

#if defined(_WIN32)
  for (const auto &item : spec.environment) {
    const std::wstring key(item.first.begin(), item.first.end());
    const std::wstring value(item.second.begin(), item.second.end());
    _wputenv_s(key.c_str(), value.c_str());
  }
  std::wstring command_line;
  for (const auto &arg : spec.arguments) {
    if (!command_line.empty()) command_line.push_back(L' ');
    command_line += QuoteWindowsArg(std::filesystem::u8path(arg).wstring());
  }
  std::vector<wchar_t> command(command_line.begin(), command_line.end());
  command.push_back(0);
  HANDLE log = CreateFileW(spec.log_path.c_str(), GENERIC_WRITE, FILE_SHARE_READ, nullptr, CREATE_ALWAYS, FILE_ATTRIBUTE_NORMAL, nullptr);
  STARTUPINFOW startup{}; startup.cb = sizeof(startup);
  if (log != INVALID_HANDLE_VALUE) {
    startup.dwFlags = STARTF_USESTDHANDLES;
    startup.hStdOutput = log;
    startup.hStdError = log;
    startup.hStdInput = GetStdHandle(STD_INPUT_HANDLE);
  }
  PROCESS_INFORMATION process{};
  const BOOL created = CreateProcessW(spec.executable.c_str(), command.data(), nullptr, nullptr, TRUE, 0, nullptr,
                                      spec.working_directory.c_str(), &startup, &process);
  if (log != INVALID_HANDLE_VALUE) CloseHandle(log);
  if (!created) { result.status = LaunchStatus::ExecFailure; return result; }
  WaitForSingleObject(process.hProcess, INFINITE);
  DWORD exit_code = 0; GetExitCodeProcess(process.hProcess, &exit_code);
  CloseHandle(process.hThread); CloseHandle(process.hProcess);
  result.exit_code = static_cast<int>(exit_code);
  result.status = exit_code == 0 ? LaunchStatus::NormalExit : LaunchStatus::CoreError;
  return result;
#elif defined(__linux__)
  std::vector<std::string> args = spec.arguments;
  std::vector<char *> argv;
  for (std::string &arg : args) argv.push_back(const_cast<char *>(arg.c_str()));
  argv.push_back(nullptr);
  const pid_t child = fork();
  if (child < 0) { result.status = LaunchStatus::ExecFailure; return result; }
  if (child == 0) {
    for (const auto &item : spec.environment) setenv(item.first.c_str(), item.second.c_str(), 1);
    if (chdir(spec.working_directory.c_str()) != 0) _exit(126);
    const int log = open(spec.log_path.c_str(), O_WRONLY | O_CREAT | O_TRUNC, 0644);
    if (log >= 0) { dup2(log, STDOUT_FILENO); dup2(log, STDERR_FILENO); close(log); }
    execv(spec.executable.c_str(), argv.data());
    _exit(127);
  }
  int status = 0;
  while (waitpid(child, &status, 0) < 0) if (errno != EINTR) { result.status = LaunchStatus::ExecFailure; return result; }
  if (WIFSIGNALED(status)) { result.status = LaunchStatus::Signaled; result.signal = WTERMSIG(status); return result; }
  result.exit_code = WIFEXITED(status) ? WEXITSTATUS(status) : -1;
  result.status = result.exit_code == 0 ? LaunchStatus::NormalExit :
                  (result.exit_code == 126 || result.exit_code == 127 ? LaunchStatus::ExecFailure : LaunchStatus::CoreError);
  return result;
#else
  (void)config; (void)game;
  result.status = LaunchStatus::Unsupported; return result;
#endif
}

std::string DescribeLaunchResult(const LaunchResult &result) {
  switch (result.status) {
    case LaunchStatus::NormalExit: return {};
    case LaunchStatus::MissingCore: return u8"核心文件不存在";
    case LaunchStatus::InvalidGame: return u8"游戏入口无效，请检查 game.ini 的 entry";
    case LaunchStatus::ExecFailure: return u8"核心启动失败";
    case LaunchStatus::CoreError: return u8"核心异常退出，代码 " + std::to_string(result.exit_code);
    case LaunchStatus::Signaled: return u8"核心被信号终止，信号 " + std::to_string(result.signal);
    default: return u8"当前平台不支持该核心";
  }
}

bool WriteLaunchRequest(const AppConfig &config, const GameEntry &game) {
  std::error_code ec;
  const auto cache_dir = config.root / "cache";
  std::filesystem::create_directories(cache_dir, ec);
  std::filesystem::create_directories(game.save_path, ec);
  std::ofstream out(cache_dir / "launch_request.ini", std::ios::trunc);
  if (!out) return false;
  out << "core=" << CoreKindName(game.core) << "\npath=" << game.path.u8string()
      << "\nsave=" << game.save_path.u8string() << "\nentry=" << game.entry_point.u8string()
      << "\nencoding=" << EffectiveString(game.overrides.encoding, "gbk")
      << "\naspect=" << EffectiveString(game.overrides.aspect, config.default_aspect)
      << "\nfilter=" << EffectiveString(game.overrides.filter, config.default_filter)
      << "\nvirtual_mouse=" << ((game.overrides.has_virtual_mouse ? game.overrides.virtual_mouse : config.virtual_mouse) ? "1" : "0")
      << "\nmouse_speed=" << (game.overrides.mouse_speed > 0 ? game.overrides.mouse_speed : config.mouse_speed)
      << "\nmouse_accel=" << (game.overrides.mouse_accel > 0.0f ? game.overrides.mouse_accel : config.mouse_accel) << "\n";
  return true;
}

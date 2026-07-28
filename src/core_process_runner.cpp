#include "core_process_runner.h"

#include <chrono>
#include <cstdlib>
#include <fstream>
#include <system_error>
#include <thread>

#if defined(__linux__)
#include <cerrno>
#include <fcntl.h>
#include <signal.h>
#include <sys/wait.h>
#include <unistd.h>
#elif defined(_WIN32)
#define WIN32_LEAN_AND_MEAN
#include <windows.h>
#endif

namespace {
#if defined(__linux__)
std::string QuoteShellArg(const std::string &value) {
  std::string result = "'";
  for (char ch : value) {
    if (ch == '\'') result += "'\\''";
    else result.push_back(ch);
  }
  result.push_back('\'');
  return result;
}

std::filesystem::path FindSwaySocket() {
  std::vector<std::filesystem::path> candidates;
  if (const char *socket = std::getenv("SWAYSOCK"); socket && *socket)
    candidates.emplace_back(socket);
  if (const char *runtime = std::getenv("XDG_RUNTIME_DIR"); runtime && *runtime)
    candidates.emplace_back(std::filesystem::path(runtime) / "sway-ipc.0.sock");
  candidates.emplace_back("/run/0-runtime-dir/sway-ipc.0.sock");
  candidates.emplace_back("/var/run/0-runtime-dir/sway-ipc.0.sock");
  std::error_code ec;
  for (const auto &candidate : candidates) {
    if (std::filesystem::exists(candidate, ec)) return candidate;
    ec.clear();
  }
  return {};
}

int Krkr2DisplayDimension(const CoreLaunchSpec &spec, const char *name,
                          int fallback) {
  const auto item = spec.environment.find(name);
  if (item == spec.environment.end()) return fallback;
  char *end = nullptr;
  const long value = std::strtol(item->second.c_str(), &end, 10);
  if (end == item->second.c_str() || *end != '\0' || value < 320 ||
      value > 8192) return fallback;
  return static_cast<int>(value);
}

std::string Krkr2EnvironmentValue(const CoreLaunchSpec &spec,
                                  const char *name,
                                  const char *fallback) {
  const auto item = spec.environment.find(name);
  return item == spec.environment.end() ? std::string(fallback) : item->second;
}

bool FullscreenKrkr2Xwayland(const std::filesystem::path &sway_socket,
                             const std::string &display) {
  const std::string criteria = "[title=\"Xwayland on " + display + "\"]";
  for (int attempt = 0; attempt < 20; ++attempt) {
    const pid_t child = fork();
    if (child < 0) return false;
    if (child == 0) {
      execlp("swaymsg", "swaymsg", "-q", "-s", sway_socket.c_str(),
             criteria.c_str(), "fullscreen", "enable",
             static_cast<char *>(nullptr));
      _exit(127);
    }
    int status = 0;
    while (waitpid(child, &status, 0) < 0) {
      if (errno != EINTR) return false;
    }
    if (WIFEXITED(status) && WEXITSTATUS(status) == 0) return true;
    std::this_thread::sleep_for(std::chrono::milliseconds(100));
  }
  return false;
}

bool EnsureKrkr2Xwayland(const CoreLaunchSpec &spec) {
  const auto requested = spec.environment.find("ROCGALGAME_KRKR_XWAYLAND");
  if (requested == spec.environment.end() || requested->second != "1") return true;
  const auto display_it = spec.environment.find("DISPLAY");
  if (display_it == spec.environment.end() || display_it->second.size() < 2 ||
      display_it->second.front() != ':') return false;
  const std::string display_number = display_it->second.substr(1);
  if (display_number.find_first_not_of("0123456789") != std::string::npos) return false;
  const auto x_socket = std::filesystem::path("/tmp/.X11-unix") /
                        ("X" + display_number);
  const auto sway_socket = FindSwaySocket();
  if (sway_socket.empty()) return false;
  std::error_code ec;
  if (std::filesystem::exists(x_socket, ec))
    return FullscreenKrkr2Xwayland(sway_socket, display_it->second);
  const auto libraries = spec.environment.find("LD_LIBRARY_PATH");
  const std::string library_path = libraries == spec.environment.end()
                                       ? std::string{} : libraries->second;
  const int width = Krkr2DisplayDimension(
      spec, "ROCGALGAME_KRKR_XWAYLAND_WIDTH", 960);
  const int height = Krkr2DisplayDimension(
      spec, "ROCGALGAME_KRKR_XWAYLAND_HEIGHT", 640);
  const bool accelerated =
      Krkr2EnvironmentValue(spec, "ROCGALGAME_KRKR_XWAYLAND_RENDERER",
                            "software") == "hardware";
  const bool use_shm =
      Krkr2EnvironmentValue(spec, "ROCGALGAME_KRKR_XWAYLAND_SHM", "1") != "0";
  const std::string command =
      "env " + QuoteShellArg("LD_LIBRARY_PATH=" + library_path) +
      (accelerated ? " LIBGL_ALWAYS_SOFTWARE=0"
                   : " LIBGL_ALWAYS_SOFTWARE=1") +
      " Xwayland " + QuoteShellArg(display_it->second) +
      " -ac -terminate -geometry " + std::to_string(width) + "x" +
      std::to_string(height) + (accelerated ? "" : " -glamor off") +
      (use_shm ? " -shm" : "");

  const pid_t starter = fork();
  if (starter < 0) return false;
  if (starter == 0) {
    execlp("swaymsg", "swaymsg", "-s", sway_socket.c_str(), "exec",
           command.c_str(), static_cast<char *>(nullptr));
    _exit(127);
  }
  int status = 0;
  while (waitpid(starter, &status, 0) < 0) {
    if (errno != EINTR) return false;
  }
  if (!WIFEXITED(status) || WEXITSTATUS(status) != 0) return false;
  for (int attempt = 0; attempt < 50; ++attempt) {
    ec.clear();
    if (std::filesystem::exists(x_socket, ec))
      return FullscreenKrkr2Xwayland(sway_socket, display_it->second);
    std::this_thread::sleep_for(std::chrono::milliseconds(100));
  }
  return false;
}

void SignalProcessGroup(pid_t child, int signal) {
  if (kill(-child, signal) != 0 && errno == ESRCH) kill(child, signal);
}
#endif

#ifdef _WIN32
std::wstring QuoteWindowsArg(const std::wstring &arg) {
  if (arg.find_first_of(L" \t\"") == std::wstring::npos) return arg;
  std::wstring out = L"\"";
  unsigned slashes = 0;
  for (wchar_t value : arg) {
    if (value == L'\\') {
      ++slashes;
      continue;
    }
    if (value == L'\"') out.append(slashes * 2 + 1, L'\\');
    else out.append(slashes, L'\\');
    slashes = 0;
    out.push_back(value);
  }
  out.append(slashes * 2, L'\\');
  out.push_back(L'\"');
  return out;
}
#endif
}  // namespace

LaunchResult CoreProcessRunner::Run(const CoreLaunchSpec &spec,
                                    const CorePollCallback &poll) const {
  LaunchResult result;
  result.log_path = spec.log_path;
  std::error_code ec;
  std::filesystem::create_directories(spec.save_path, ec);
  std::filesystem::create_directories(spec.log_path.parent_path(), ec);
  if (!std::filesystem::is_regular_file(spec.executable, ec)) {
    result.status = LaunchStatus::MissingCore;
    return result;
  }
#if defined(_WIN32)
  for (const auto &item : spec.environment) {
    const std::wstring key(item.first.begin(), item.first.end());
    const std::wstring value(item.second.begin(), item.second.end());
    _wputenv_s(key.c_str(), value.c_str());
  }
  std::wstring command_line;
  for (const std::string &arg : spec.arguments) {
    if (!command_line.empty()) command_line.push_back(L' ');
    command_line += QuoteWindowsArg(std::filesystem::u8path(arg).wstring());
  }
  std::vector<wchar_t> command(command_line.begin(), command_line.end());
  command.push_back(0);
  HANDLE log = CreateFileW(spec.log_path.c_str(), GENERIC_WRITE, FILE_SHARE_READ,
                           nullptr, CREATE_ALWAYS, FILE_ATTRIBUTE_NORMAL, nullptr);
  STARTUPINFOW startup{};
  startup.cb = sizeof(startup);
  if (log != INVALID_HANDLE_VALUE) {
    startup.dwFlags = STARTF_USESTDHANDLES;
    startup.hStdOutput = log;
    startup.hStdError = log;
    startup.hStdInput = GetStdHandle(STD_INPUT_HANDLE);
  }
  PROCESS_INFORMATION process{};
  const BOOL created = CreateProcessW(spec.executable.c_str(), command.data(), nullptr,
                                      nullptr, TRUE, 0, nullptr,
                                      spec.working_directory.c_str(), &startup, &process);
  if (log != INVALID_HANDLE_VALUE) CloseHandle(log);
  if (!created) {
    result.status = LaunchStatus::ExecFailure;
    return result;
  }
  bool user_terminated = false;
  while (WaitForSingleObject(process.hProcess, 12) == WAIT_TIMEOUT) {
    if (poll && poll()) {
      user_terminated = true;
      TerminateProcess(process.hProcess, 0);
      WaitForSingleObject(process.hProcess, 2000);
      break;
    }
  }
  DWORD exit_code = 0;
  GetExitCodeProcess(process.hProcess, &exit_code);
  CloseHandle(process.hThread);
  CloseHandle(process.hProcess);
  result.exit_code = user_terminated ? 0 : static_cast<int>(exit_code);
  if (user_terminated) {
    result.status = LaunchStatus::NormalExit;
    result.detail = "user requested exit";
    return result;
  }
  result.status = exit_code == 0 ? LaunchStatus::NormalExit : LaunchStatus::CoreError;
  return result;
#elif defined(__linux__)
  if (!EnsureKrkr2Xwayland(spec)) {
    std::ofstream log(spec.log_path, std::ios::trunc);
    log << "KRKR2 private Xwayland setup failed\n";
    result.status = LaunchStatus::ExecFailure;
    result.detail = "KRKR2 private Xwayland setup failed";
    return result;
  }
  std::vector<std::string> args = spec.arguments;
  std::vector<char *> argv;
  for (std::string &arg : args) argv.push_back(const_cast<char *>(arg.c_str()));
  argv.push_back(nullptr);
  const pid_t child = fork();
  if (child < 0) {
    result.status = LaunchStatus::ExecFailure;
    return result;
  }
  if (child == 0) {
    setpgid(0, 0);
    for (const auto &item : spec.environment) {
      setenv(item.first.c_str(), item.second.c_str(), 1);
    }
    if (chdir(spec.working_directory.c_str()) != 0) _exit(126);
    const int log = open(spec.log_path.c_str(), O_WRONLY | O_CREAT | O_TRUNC, 0644);
    if (log >= 0) {
      dup2(log, STDOUT_FILENO);
      dup2(log, STDERR_FILENO);
      close(log);
    }
    execv(spec.executable.c_str(), argv.data());
    _exit(127);
  }
  setpgid(child, child);
  int status = 0;
  bool user_terminated = false;
  bool sent_kill = false;
  auto terminate_deadline = std::chrono::steady_clock::time_point::max();
  while (true) {
    const pid_t waited = waitpid(child, &status, WNOHANG);
    if (waited == child) break;
    if (waited < 0 && errno != EINTR) {
      result.status = LaunchStatus::ExecFailure;
      return result;
    }
    const auto now = std::chrono::steady_clock::now();
    if (!user_terminated && poll && poll()) {
      user_terminated = true;
      terminate_deadline = now + std::chrono::milliseconds(1500);
      SignalProcessGroup(child, SIGTERM);
    } else if (user_terminated && !sent_kill && now >= terminate_deadline) {
      sent_kill = true;
      SignalProcessGroup(child, SIGKILL);
    }
    std::this_thread::sleep_for(std::chrono::milliseconds(12));
  }
  if (user_terminated) {
    result.status = LaunchStatus::NormalExit;
    result.exit_code = 0;
    result.detail = "user requested exit";
    return result;
  }
  if (WIFSIGNALED(status)) {
    result.status = LaunchStatus::Signaled;
    result.signal = WTERMSIG(status);
    return result;
  }
  result.exit_code = WIFEXITED(status) ? WEXITSTATUS(status) : -1;
  result.status = result.exit_code == 0 ? LaunchStatus::NormalExit
                  : (result.exit_code == 126 || result.exit_code == 127)
                      ? LaunchStatus::ExecFailure : LaunchStatus::CoreError;
  return result;
#else
  result.status = LaunchStatus::Unsupported;
  return result;
#endif
}

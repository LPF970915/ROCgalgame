#include "core_process_runner.h"

#include <cstdlib>
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

LaunchResult CoreProcessRunner::Run(const CoreLaunchSpec &spec) const {
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
  WaitForSingleObject(process.hProcess, INFINITE);
  DWORD exit_code = 0;
  GetExitCodeProcess(process.hProcess, &exit_code);
  CloseHandle(process.hThread);
  CloseHandle(process.hProcess);
  result.exit_code = static_cast<int>(exit_code);
  result.status = exit_code == 0 ? LaunchStatus::NormalExit : LaunchStatus::CoreError;
  return result;
#elif defined(__linux__)
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
  int status = 0;
  while (waitpid(child, &status, 0) < 0) {
    if (errno != EINTR) {
      result.status = LaunchStatus::ExecFailure;
      return result;
    }
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

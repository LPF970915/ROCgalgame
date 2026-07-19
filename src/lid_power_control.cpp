#include "lid_power_control.h"

#include <cstdlib>
#include <initializer_list>
#include <utility>

namespace {
std::string EscapeShellArg(const std::string &text) {
  std::string escaped = "'";
  for (char ch : text) escaped += ch == '\'' ? "'\\''" : std::string(1, ch);
  return escaped + "'";
}

bool RunQuiet(const std::string &command) {
  if (command.empty()) return false;
#ifdef _WIN32
  return std::system(command.c_str()) == 0;
#else
  return std::system((command + " >/dev/null 2>&1").c_str()) == 0;
#endif
}

const char *FirstEnv(const char *primary, const char *compatible) {
  const char *value = std::getenv(primary);
  if (value && *value) return value;
  value = compatible ? std::getenv(compatible) : nullptr;
  return value && *value ? value : nullptr;
}

bool RunScriptArgs(const std::filesystem::path &script,
                   std::initializer_list<const char *> args) {
  std::error_code ec;
  if (script.empty() || !std::filesystem::exists(script, ec) || ec) return false;
  for (const char *arg : args) {
    if (RunQuiet(EscapeShellArg(script.string()) + " " + EscapeShellArg(arg))) return true;
  }
  return false;
}

bool SetGkdDisplayPower(bool on) {
  const char *custom = FirstEnv(on ? "ROCGALGAME_SCREEN_ON_COMMAND"
                                   : "ROCGALGAME_SCREEN_OFF_COMMAND",
                                on ? "ROCREADER_GKD350H_SCREEN_ON_COMMAND"
                                   : "ROCREADER_GKD350H_SCREEN_OFF_COMMAND");
  if (custom) return RunQuiet(custom);
#ifdef _WIN32
  return false;
#else
  const char *output_env = FirstEnv("ROCGALGAME_GKD350H_OUTPUT", "ROCREADER_GKD350H_OUTPUT");
  const std::string output = output_env ? output_env : "DSI-1";
  return RunQuiet(
      "env XDG_RUNTIME_DIR=/var/run/0-runtime-dir WAYLAND_DISPLAY=wayland-1 "
      "SWAYSOCK=/var/run/0-runtime-dir/sway-ipc.0.sock swaymsg output " +
      EscapeShellArg(output) + " power " + (on ? "on" : "off"));
#endif
}
}  // namespace

LidPowerController::LidPowerController(std::filesystem::path power_script_path)
    : power_script_path_(std::move(power_script_path)) {}

bool LidPowerController::TriggerAutoIfEnabled() const {
  if (const char *command = FirstEnv("ROCGALGAME_AUTO_SLEEP_COMMAND",
                                     "ROCREADER_RGDS_AUTO_SLEEP_COMMAND")) {
    return RunQuiet(command);
  }
  return RunScriptArgs(power_script_path_, {"auto", "powerkey", "manual", "off"});
}

bool LidPowerController::TriggerPowerKeyScreenOff(InputProfile input_profile) const {
  if (input_profile == InputProfile::GKD350HUltra) return SetGkdDisplayPower(false);
  if (const char *command = FirstEnv("ROCGALGAME_SCREEN_OFF_COMMAND", nullptr)) {
    return RunQuiet(command);
  }
  return RunScriptArgs(power_script_path_, {"off", "manual", "powerkey"});
}

bool LidPowerController::TriggerScreenOn(InputProfile input_profile) const {
  if (input_profile == InputProfile::GKD350HUltra) return SetGkdDisplayPower(true);
  if (const char *command = FirstEnv("ROCGALGAME_SCREEN_ON_COMMAND", nullptr)) {
    return RunQuiet(command);
  }
  return RunScriptArgs(power_script_path_, {"on", "wake", "resume"});
}

std::string LidPowerController::PowerScriptPath() const { return power_script_path_.string(); }

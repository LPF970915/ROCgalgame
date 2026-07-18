#include "config.h"

#include <algorithm>
#include <cctype>
#include <cstdlib>
#include <fstream>
#include <sstream>
#include <vector>

namespace {
std::filesystem::path ExeDir(const char *argv0) {
  std::error_code ec;
  if (const char *root = std::getenv("ROCGALGAME_ROOT"); root && *root) {
    return std::filesystem::absolute(root, ec);
  }
  if (argv0 && *argv0) {
    std::filesystem::path p(argv0);
    if (p.has_parent_path()) return std::filesystem::absolute(p.parent_path(), ec);
  }
  return std::filesystem::current_path(ec);
}

void ParseConfigLine(AppConfig &config, const std::string &raw_line) {
  std::string line = Trim(raw_line);
  if (line.empty() || line[0] == '#') return;
  const size_t eq = line.find('=');
  if (eq == std::string::npos) return;
  const std::string key = ToLowerAscii(Trim(line.substr(0, eq)));
  const std::string value = Trim(line.substr(eq + 1));
  if (key == "screen_profile") {
    config.screen_profile = value;
    if (value == "1600x1440" || ToLowerAscii(value) == "gkd350h-ultra" || ToLowerAscii(value) == "gkd350h") {
      config.screen_w = 1600;
      config.screen_h = 1440;
    }
  } else if (key == "input_profile") {
    config.input_profile = value;
  } else if (key == "games_root") {
    config.games_root = value;
  } else if (key == "covers_root") {
    config.covers_root = value;
  } else if (key == "saves_root") {
    config.saves_root = value;
  } else if (key == "default_aspect") {
    config.default_aspect = value;
  } else if (key == "default_filter") {
    config.default_filter = value;
  } else if (key == "system_language") {
    config.system_language = value.empty() ? "zh" : value;
  } else if (key == "key_sound") {
    config.key_sound = IsTruthy(value);
  } else if (key == "system_volume_percent") {
    try { config.system_volume_percent = std::clamp(std::stoi(value), 0, 100); } catch (...) {}
  } else if (key == "brightness_level") {
    try { config.brightness_level = std::clamp(std::stoi(value), 0, 8); } catch (...) {}
  } else if (key == "auto_sleep_minutes") {
    try { config.auto_sleep_minutes = std::max(0, std::stoi(value)); } catch (...) {}
  } else if (key == "lid_close_screen_off" || key == "auto_sleep_enabled") {
    config.auto_sleep_enabled = IsTruthy(value);
  } else if (key == "auto_sleep_interval_index") {
    try { config.auto_sleep_interval_index = std::clamp(std::stoi(value), 0, 5); } catch (...) {}
  } else if (key == "selected_contributor_avatar_label" || key == "selected_avatar") {
    config.selected_avatar = value;
  } else if (key == "virtual_mouse") {
    config.virtual_mouse = IsTruthy(value);
  } else if (key == "mouse_speed") {
    try { config.mouse_speed = std::max(1, std::stoi(value)); } catch (...) {}
  } else if (key == "mouse_accel") {
    try { config.mouse_accel = std::max(0.1f, std::stof(value)); } catch (...) {}
  } else if (key == "update_manifest_url") {
    config.update_manifest_url = value;
  }
}
}  // namespace

std::string Trim(std::string value) {
  while (!value.empty() && std::isspace(static_cast<unsigned char>(value.front())) != 0) value.erase(value.begin());
  while (!value.empty() && std::isspace(static_cast<unsigned char>(value.back())) != 0) value.pop_back();
  return value;
}

std::string ToLowerAscii(std::string value) {
  std::transform(value.begin(), value.end(), value.begin(), [](unsigned char c) { return static_cast<char>(std::tolower(c)); });
  return value;
}

bool IsTruthy(const std::string &value) {
  const std::string v = ToLowerAscii(Trim(value));
  return v == "1" || v == "true" || v == "yes" || v == "on";
}

AppConfig LoadAppConfig(const char *argv0) {
  AppConfig config;
  config.root = ExeDir(argv0);

  std::vector<std::filesystem::path> candidates = {
      config.root / "native_config.ini",
      std::filesystem::current_path() / "native_config.ini",
  };
  for (const auto &path : candidates) {
    std::ifstream in(path);
    if (!in) continue;
    std::string line;
    while (std::getline(in, line)) ParseConfigLine(config, line);
    break;
  }

  if (const char *profile = std::getenv("ROCGALGAME_SCREEN_PROFILE"); profile && *profile) {
    config.screen_profile = profile;
    if (config.screen_profile == "1600x1440") {
      config.screen_w = 1600;
      config.screen_h = 1440;
    }
  }
  config.default_aspect = ToLowerAscii(Trim(config.default_aspect));
  if (config.default_aspect == "fit-width") config.default_aspect = "contain";
  if (config.default_aspect != "stretch" && config.default_aspect != "fill-height" &&
      config.default_aspect != "contain") {
    config.default_aspect = "contain";
  }
  return config;
}

bool SaveAppConfig(const AppConfig &config) {
  std::error_code ec;
  std::filesystem::create_directories(config.root, ec);
  std::ofstream out(config.root / "native_config.ini", std::ios::trunc);
  if (!out) return false;
  out << "screen_profile=" << config.screen_profile << "\n";
  out << "input_profile=" << config.input_profile << "\n";
  out << "games_root=" << config.games_root.u8string() << "\n";
  out << "covers_root=" << config.covers_root.u8string() << "\n";
  out << "saves_root=" << config.saves_root.u8string() << "\n";
  out << "default_aspect=" << config.default_aspect << "\n";
  out << "default_filter=" << config.default_filter << "\n";
  out << "system_language=" << config.system_language << "\n";
  out << "key_sound=" << (config.key_sound ? "1" : "0") << "\n";
  out << "system_volume_percent=" << config.system_volume_percent << "\n";
  out << "brightness_level=" << config.brightness_level << "\n";
  out << "auto_sleep_minutes=" << config.auto_sleep_minutes << "\n";
  out << "lid_close_screen_off=" << (config.auto_sleep_enabled ? "1" : "0") << "\n";
  out << "auto_sleep_interval_index=" << config.auto_sleep_interval_index << "\n";
  if (!config.selected_avatar.empty()) out << "selected_contributor_avatar_label=" << config.selected_avatar << "\n";
  out << "virtual_mouse=" << (config.virtual_mouse ? "1" : "0") << "\n";
  out << "mouse_speed=" << config.mouse_speed << "\n";
  out << "mouse_accel=" << config.mouse_accel << "\n";
  out << "update_manifest_url=" << config.update_manifest_url << "\n";
  return true;
}

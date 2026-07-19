#include "app_stores.h"

#include <algorithm>
#include <fstream>
#include <iomanip>
#include <sstream>
#include <unordered_map>
#include <unordered_set>
#include <utility>

namespace {
std::string ParsedKey(const std::string &line) {
  const std::string trimmed = Trim(line);
  if (trimmed.empty() || trimmed.front() == '#') return {};
  const size_t eq = trimmed.find('=');
  if (eq == std::string::npos) return {};
  return ToLowerAscii(Trim(trimmed.substr(0, eq)));
}

std::string CanonicalKey(const std::string &key) {
  if (key == "auto_sleep_enabled") return "lid_close_screen_off";
  if (key == "selected_avatar") return "selected_contributor_avatar_label";
  static const std::unordered_set<std::string> known = {
      "screen_profile", "input_profile", "games_root", "covers_root", "saves_root",
      "default_aspect", "default_filter", "system_language", "key_sound",
      "system_volume_percent", "brightness_level", "auto_sleep_minutes",
      "lid_close_screen_off", "auto_sleep_interval_index",
      "selected_contributor_avatar_label", "virtual_mouse", "mouse_speed",
      "mouse_accel", "update_manifest_url",
  };
  return known.count(key) != 0 ? key : std::string{};
}

std::vector<std::pair<std::string, std::string>> SerializedValues(const AppConfig &config) {
  std::ostringstream accel;
  accel << std::fixed << std::setprecision(2) << config.mouse_accel;
  return {
      {"screen_profile", config.screen_profile},
      {"input_profile", config.input_profile},
      {"games_root", config.games_root.u8string()},
      {"covers_root", config.covers_root.u8string()},
      {"saves_root", config.saves_root.u8string()},
      {"default_aspect", config.default_aspect},
      {"default_filter", config.default_filter},
      {"system_language", config.system_language},
      {"key_sound", config.key_sound ? "1" : "0"},
      {"system_volume_percent", std::to_string(config.system_volume_percent)},
      {"brightness_level", std::to_string(config.brightness_level)},
      {"auto_sleep_minutes", std::to_string(config.auto_sleep_minutes)},
      {"lid_close_screen_off", config.auto_sleep_enabled ? "1" : "0"},
      {"auto_sleep_interval_index", std::to_string(config.auto_sleep_interval_index)},
      {"selected_contributor_avatar_label", config.selected_avatar},
      {"virtual_mouse", config.virtual_mouse ? "1" : "0"},
      {"mouse_speed", std::to_string(config.mouse_speed)},
      {"mouse_accel", accel.str()},
      {"update_manifest_url", config.update_manifest_url},
  };
}
}  // namespace

void ConfigStore::Load(const char *argv0) {
  config_ = LoadAppConfig(argv0);
  path_ = config_.root / "native_config.ini";
  std::filesystem::path source_path = path_;
  if (!std::filesystem::exists(source_path)) {
    const std::filesystem::path cwd_path = std::filesystem::current_path() / "native_config.ini";
    if (std::filesystem::exists(cwd_path)) source_path = cwd_path;
  }
  LoadDocument(source_path);
}

void ConfigStore::LoadFromPath(const std::filesystem::path &path) {
  path_ = path;
  config_ = LoadAppConfigFromFile(path, path.parent_path());
  LoadDocument(path);
}

const AppConfig &ConfigStore::Get() const { return config_; }
AppConfig &ConfigStore::Mutable() { return config_; }
bool ConfigStore::IsDirty() const { return dirty_; }

bool ConfigStore::ShouldFlush(uint32_t now, uint32_t delay_ms) const {
  return dirty_ && (last_dirty_tick_ == 0 || now - last_dirty_tick_ >= delay_ms);
}

void ConfigStore::MarkDirty(uint32_t now) {
  dirty_ = true;
  last_dirty_tick_ = now;
}

bool ConfigStore::Save() {
  std::error_code ec;
  std::filesystem::create_directories(path_.parent_path(), ec);
  std::ofstream out(path_, std::ios::trunc);
  if (!out) return false;

  const auto serialized = SerializedValues(config_);
  std::unordered_map<std::string, std::string> values;
  for (const auto &entry : serialized) values.emplace(entry.first, entry.second);

  std::unordered_set<std::string> written;
  for (const std::string &line : original_lines_) {
    const std::string parsed = ParsedKey(line);
    const std::string canonical = CanonicalKey(parsed);
    if (canonical.empty()) {
      out << line << "\n";
      continue;
    }
    if (!written.insert(canonical).second) continue;
    out << canonical << "=" << values.at(canonical) << "\n";
  }
  for (const auto &entry : serialized) {
    if (written.insert(entry.first).second) out << entry.first << "=" << entry.second << "\n";
  }
  out.close();
  if (!out.good()) return false;

  dirty_ = false;
  last_dirty_tick_ = 0;
  LoadDocument(path_);
  return true;
}

void ConfigStore::LoadDocument(const std::filesystem::path &source_path) {
  original_lines_.clear();
  bool needs_migration = false;
  std::ifstream in(source_path);
  std::string line;
  while (std::getline(in, line)) {
    const std::string key = ParsedKey(line);
    if (key == "auto_sleep_enabled" || key == "selected_avatar") needs_migration = true;
    if (key == "default_aspect") {
      const size_t eq = line.find('=');
      if (eq != std::string::npos && ToLowerAscii(Trim(line.substr(eq + 1))) == "fit-width") {
        needs_migration = true;
      }
    }
    original_lines_.push_back(line);
  }
  dirty_ = needs_migration;
  last_dirty_tick_ = 0;
}

#include "system_status.h"

#include <algorithm>
#include <cctype>
#include <cstdlib>
#include <ctime>
#include <fstream>
#include <iomanip>
#include <iostream>
#include <sstream>

namespace {
bool VerboseLogEnabled() {
  auto enabled = [](const char *value) {
    return value && *value && std::string(value) != "0";
  };
  return enabled(std::getenv("ROCREADER_VERBOSE_LOG")) || enabled(std::getenv("ROCREADER_DEBUG_LOG"));
}

std::string ReadSmallTextFileImpl(const std::filesystem::path &path) {
  std::ifstream in(path);
  if (!in) return {};
  std::ostringstream oss;
  oss << in.rdbuf();
  return oss.str();
}

std::string TrimAsciiImpl(std::string text) {
  while (!text.empty() && std::isspace(static_cast<unsigned char>(text.front())) != 0) {
    text.erase(text.begin());
  }
  while (!text.empty() && std::isspace(static_cast<unsigned char>(text.back())) != 0) {
    text.pop_back();
  }
  return text;
}

std::string ToLowerAsciiImpl(std::string text) {
  std::transform(text.begin(), text.end(), text.begin(),
                 [](unsigned char ch) { return static_cast<char>(std::tolower(ch)); });
  return text;
}

bool EnvTruthy(const char *value) {
  if (!value || !*value) return false;
  const std::string lower = ToLowerAsciiImpl(TrimAsciiImpl(value));
  return lower == "1" || lower == "true" || lower == "yes" || lower == "on";
}

bool IsGkd350HUltraRuntime() {
  const std::string model =
      ToLowerAsciiImpl(std::getenv("ROCGALGAME_DEVICE_MODEL") ? std::getenv("ROCGALGAME_DEVICE_MODEL") : "");
  const std::string profile =
      ToLowerAsciiImpl(std::getenv("ROCGALGAME_SCREEN_PROFILE") ? std::getenv("ROCGALGAME_SCREEN_PROFILE") : "");
  const std::string width =
      TrimAsciiImpl(std::getenv("ROCREADER_SCREEN_W") ? std::getenv("ROCREADER_SCREEN_W") : "");
  const std::string height =
      TrimAsciiImpl(std::getenv("ROCREADER_SCREEN_H") ? std::getenv("ROCREADER_SCREEN_H") : "");
  return model.find("gkd350h") != std::string::npos ||
         model.find("gkd-350h") != std::string::npos ||
         model.find("gkd_atom") != std::string::npos ||
         model.find("gkd-atom") != std::string::npos ||
         profile == "1600x1440" ||
         (width == "1600" && height == "1440");
}
} // namespace

SystemStatusMonitor::SystemStatusMonitor() {
  DiscoverBatteryPaths();
  UpdateClock();
  UpdateBattery();
}

void SystemStatusMonitor::Poll(uint32_t now) {
  if (last_poll_tick_ != 0 && now - last_poll_tick_ < 500) return;
  last_poll_tick_ = now;
  UpdateClock();
  UpdateBattery();
}

const SystemStatusSnapshot &SystemStatusMonitor::Snapshot() const { return snapshot_; }

std::string SystemStatusMonitor::BatteryCapacityPath() const { return battery_capacity_path_.string(); }

std::string SystemStatusMonitor::BatteryStatusPath() const { return battery_status_path_.string(); }

std::string SystemStatusMonitor::ReadSmallTextFile(const std::filesystem::path &path) {
  return ReadSmallTextFileImpl(path);
}

std::string SystemStatusMonitor::TrimAscii(std::string text) {
  return TrimAsciiImpl(std::move(text));
}

std::string SystemStatusMonitor::ToLowerAscii(std::string text) {
  return ToLowerAsciiImpl(std::move(text));
}

bool SystemStatusMonitor::ParseInt(const std::string &text, int &out_value) {
  try {
    size_t consumed = 0;
    const int value = std::stoi(text, &consumed);
    if (consumed == 0) return false;
    out_value = value;
    return true;
  } catch (...) {
    return false;
  }
}

void SystemStatusMonitor::DiscoverBatteryPaths() {
  const char *env_capacity = std::getenv("ROCREADER_BATTERY_CAPACITY_PATH");
  const char *env_status = std::getenv("ROCREADER_BATTERY_STATUS_PATH");
  const char *env_charger_status = std::getenv("ROCREADER_CHARGER_STATUS_PATH");
  const char *env_charger_online = std::getenv("ROCREADER_CHARGER_ONLINE_PATH");
  if (env_capacity && *env_capacity) battery_capacity_path_ = env_capacity;
  if (env_status && *env_status) battery_status_path_ = env_status;
  if (env_charger_status && *env_charger_status) charger_status_paths_.emplace_back(env_charger_status);
  if (env_charger_online && *env_charger_online) charger_online_paths_.emplace_back(env_charger_online);
  if (!battery_capacity_path_.empty() || !battery_status_path_.empty() ||
      !charger_status_paths_.empty() || !charger_online_paths_.empty()) {
    if (VerboseLogEnabled()) {
      std::cout << "[native_h700] battery discover: env capacity=" << battery_capacity_path_.string()
                << " status=" << battery_status_path_.string()
                << " charger_status=" << (charger_status_paths_.empty() ? "" : charger_status_paths_.front().string())
                << " charger_online=" << (charger_online_paths_.empty() ? "" : charger_online_paths_.front().string())
                << "\n";
    }
    return;
  }

  const std::filesystem::path root("/sys/class/power_supply");
  std::error_code ec;
  if (!std::filesystem::exists(root, ec) || ec) {
    if (VerboseLogEnabled()) {
      std::cout << "[native_h700] battery discover: power_supply root unavailable\n";
    }
    return;
  }

  for (std::filesystem::directory_iterator it(root, ec), end; it != end; it.increment(ec)) {
    if (ec) {
      ec.clear();
      continue;
    }
    const std::filesystem::path dir = it->path();
    const std::string type = ToLowerAscii(TrimAscii(ReadSmallTextFile(dir / "type")));
    const std::string lower_name = ToLowerAscii(dir.filename().string());
    const bool looks_like_battery =
        type == "battery" ||
        lower_name.find("battery") != std::string::npos ||
        lower_name.find("bat") != std::string::npos;
    if (!looks_like_battery) continue;

    const std::filesystem::path capacity = dir / "capacity";
    const std::filesystem::path status = dir / "status";
    if (battery_capacity_path_.empty() && std::filesystem::exists(capacity, ec) && !ec) {
      battery_capacity_path_ = capacity;
    }
    ec.clear();
    if (battery_status_path_.empty() && std::filesystem::exists(status, ec) && !ec) {
      battery_status_path_ = status;
    }
    if (!battery_capacity_path_.empty() || !battery_status_path_.empty()) break;
  }

  if (IsGkd350HUltraRuntime()) {
    for (std::filesystem::directory_iterator it(root, ec), end; it != end; it.increment(ec)) {
      if (ec) {
        ec.clear();
        continue;
      }
      const std::filesystem::path dir = it->path();
      const std::string type = ToLowerAscii(TrimAscii(ReadSmallTextFile(dir / "type")));
      const std::string lower_name = ToLowerAscii(dir.filename().string());
      const bool looks_like_charger =
          type == "usb" ||
          lower_name.find("charger") != std::string::npos ||
          lower_name.find("usb") != std::string::npos;
      if (!looks_like_charger) continue;

      const std::filesystem::path status = dir / "status";
      const std::filesystem::path online = dir / "online";
      if (std::filesystem::exists(status, ec) && !ec) charger_status_paths_.push_back(status);
      ec.clear();
      if (std::filesystem::exists(online, ec) && !ec) charger_online_paths_.push_back(online);
      ec.clear();
    }
  }

  if (VerboseLogEnabled()) {
    std::cout << "[native_h700] battery discover: capacity=" << battery_capacity_path_.string()
              << " status=" << battery_status_path_.string()
              << " charger_status_count=" << charger_status_paths_.size()
              << " charger_online_count=" << charger_online_paths_.size() << "\n";
  }
}

void SystemStatusMonitor::UpdateClock() {
  const std::time_t now = std::time(nullptr);
  std::tm local_tm{};
#if defined(_WIN32)
  localtime_s(&local_tm, &now);
#else
  localtime_r(&now, &local_tm);
#endif
  std::ostringstream oss;
  oss << std::put_time(&local_tm, "%H:%M");
  snapshot_.clock_text = oss.str();
}

void SystemStatusMonitor::UpdateBattery() {
  snapshot_.battery_available = false;
  snapshot_.battery_percent = -1;
  snapshot_.charging_status_available = false;
  snapshot_.charging = false;
  snapshot_.charging_text.clear();

  if (!battery_capacity_path_.empty()) {
    int percent = -1;
    if (ParseInt(TrimAscii(ReadSmallTextFile(battery_capacity_path_)), percent)) {
      snapshot_.battery_available = true;
      snapshot_.battery_percent = std::clamp(percent, 0, 100);
    }
  }

  if (!battery_status_path_.empty()) {
    const std::string status = TrimAscii(ReadSmallTextFile(battery_status_path_));
    if (!status.empty()) {
      const std::string lower = ToLowerAscii(status);
      snapshot_.charging_status_available = true;
      snapshot_.charging_text = status;
      snapshot_.charging =
          lower.find("charging") != std::string::npos && lower.find("discharging") == std::string::npos;
    }
  }

  for (const std::filesystem::path &path : charger_status_paths_) {
    const std::string status = TrimAscii(ReadSmallTextFile(path));
    if (status.empty()) continue;
    const std::string lower = ToLowerAscii(status);
    snapshot_.charging_status_available = true;
    if (snapshot_.charging_text.empty() || snapshot_.charging_text == "Discharging") {
      snapshot_.charging_text = status;
    }
    if (lower.find("charging") != std::string::npos && lower.find("discharging") == std::string::npos) {
      snapshot_.charging = true;
    }
  }

  for (const std::filesystem::path &path : charger_online_paths_) {
    const std::string online = TrimAscii(ReadSmallTextFile(path));
    if (online.empty()) continue;
    snapshot_.charging_status_available = true;
    if (EnvTruthy(online.c_str())) {
      snapshot_.charging = true;
      if (snapshot_.charging_text.empty() || snapshot_.charging_text == "Discharging") {
        snapshot_.charging_text = "Charging";
      }
    }
  }
}

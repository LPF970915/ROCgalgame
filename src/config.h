#pragma once

#include <filesystem>
#include <string>

struct AppConfig {
  int screen_w = 1600;
  int screen_h = 1440;
  std::string screen_profile = "1600x1440";
  std::string input_profile = "gkd350h-ultra";
  std::filesystem::path root;
  std::filesystem::path games_root = "games";
  std::filesystem::path covers_root = "covers";
  std::filesystem::path saves_root = "saves";
  std::string default_aspect = "contain";
  std::string default_filter = "reflection";
  std::string system_language = "zh";
  bool key_sound = true;
  int system_volume_percent = 50;
  int brightness_level = 8;
  int auto_sleep_minutes = 0;
  bool auto_sleep_enabled = true;
  int auto_sleep_interval_index = 2;
  std::string selected_avatar;
  bool virtual_mouse = true;
  int mouse_speed = 720;
  float mouse_accel = 1.6f;
  std::string update_manifest_url;
};

AppConfig LoadAppConfig(const char *argv0);
AppConfig LoadAppConfigFromFile(const std::filesystem::path &path,
                                const std::filesystem::path &root);
bool SaveAppConfig(const AppConfig &config);
bool SaveAppConfigToFile(const AppConfig &config, const std::filesystem::path &path);
std::string Trim(std::string value);
std::string ToLowerAscii(std::string value);
bool IsTruthy(const std::string &value);

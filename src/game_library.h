#pragma once

#include <filesystem>
#include <string>
#include <vector>

enum class CoreKind {
  Unknown,
  Ons,
  Krkr,
  Tyrano,
};

struct GameOverrides {
  std::string encoding;
  std::string aspect;
  std::string filter;
  bool has_virtual_mouse = false;
  bool virtual_mouse = true;
  int mouse_speed = 0;
  float mouse_accel = 0.0f;
};

struct GameEntry {
  CoreKind core = CoreKind::Unknown;
  std::string title;
  std::filesystem::path path;
  std::filesystem::path cover_path;
  std::filesystem::path save_path;
  GameOverrides overrides;
};

const char *CoreKindName(CoreKind kind);
std::vector<GameEntry> ScanGameLibrary(const std::filesystem::path &root,
                                       const std::filesystem::path &games_root,
                                       const std::filesystem::path &covers_root,
                                       const std::filesystem::path &saves_root);

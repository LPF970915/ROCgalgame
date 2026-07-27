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

// KRKR remains one frontend core family, but can select a compatibility
// runtime without changing the library's top-level core taxonomy.
enum class KrkrRuntime {
  Auto,
  Sdl2,
  Krkr2,
  Wine,
};

struct GameOverrides {
  std::string entry;
  KrkrRuntime krkr_runtime = KrkrRuntime::Auto;
  std::string encoding;
  std::string aspect;
  std::string filter;
  bool has_virtual_mouse = false;
  bool virtual_mouse = true;
  int mouse_speed = 0;
  float mouse_accel = 0.0f;
  int frame_limit = 0;
  std::string draw_threads;
  int graphic_cache_mb = 0;
};

struct GameEntry {
  CoreKind core = CoreKind::Unknown;
  std::string title;
  std::filesystem::path path;
  std::filesystem::path entry_point;
  std::filesystem::path cover_path;
  std::filesystem::path save_path;
  GameOverrides overrides;
};

const char *CoreKindName(CoreKind kind);
const char *KrkrRuntimeName(KrkrRuntime runtime);

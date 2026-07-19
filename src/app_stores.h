#pragma once

#include "config.h"

#include <cstdint>
#include <filesystem>
#include <string>
#include <vector>

class ConfigStore {
public:
  ConfigStore() = default;

  void Load(const char *argv0);
  void LoadFromPath(const std::filesystem::path &path);

  const AppConfig &Get() const;
  AppConfig &Mutable();

  bool IsDirty() const;
  bool ShouldFlush(uint32_t now, uint32_t delay_ms) const;
  void MarkDirty(uint32_t now);
  bool Save();

private:
  void LoadDocument(const std::filesystem::path &source_path);

  AppConfig config_;
  std::filesystem::path path_;
  std::vector<std::string> original_lines_;
  bool dirty_ = false;
  uint32_t last_dirty_tick_ = 0;
};

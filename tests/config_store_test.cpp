#include "app_stores.h"

#include <algorithm>
#include <cassert>
#include <cmath>
#include <filesystem>
#include <fstream>
#include <iostream>
#include <string>

namespace {
std::string ReadAll(const std::filesystem::path &path) {
  std::ifstream in(path, std::ios::binary);
  std::string text{std::istreambuf_iterator<char>(in), std::istreambuf_iterator<char>()};
  text.erase(std::remove(text.begin(), text.end(), '\r'), text.end());
  return text;
}

int Count(const std::string &text, const std::string &needle) {
  int count = 0;
  size_t offset = 0;
  while ((offset = text.find(needle, offset)) != std::string::npos) {
    ++count;
    offset += needle.size();
  }
  return count;
}
}  // namespace

int main() {
  const std::filesystem::path root = std::filesystem::path("build") / "config_store_test_data";
  std::error_code ec;
  std::filesystem::remove_all(root, ec);
  std::filesystem::create_directories(root, ec);
  const std::filesystem::path path = root / "native_config.ini";

  {
    std::ofstream out(path);
    out << "# user comment\n"
        << "screen_profile=1600x1440\n"
        << "input_profile=gkd350h-ultra\n"
        << "games_root=games\n"
        << "covers_root=covers\n"
        << "saves_root=saves\n"
        << "default_aspect=fill-height\n"
        << "default_filter=scanline\n"
        << "system_language=en\n"
        << "key_sound=0\n"
        << "system_volume_percent=35\n"
        << "brightness_level=6\n"
        << "auto_sleep_minutes=9\n"
        << "lid_close_screen_off=0\n"
        << "auto_sleep_interval_index=4\n"
        << "selected_contributor_avatar_label=avatar.png\n"
        << "virtual_mouse=0\n"
        << "mouse_speed=960\n"
        << "mouse_accel=2.0\n"
        << "update_manifest_url=https://example.invalid/manifest\n"
        << "future_engine_option=keep exactly\n";
  }

  ConfigStore store;
  store.LoadFromPath(path);
  const AppConfig &loaded = store.Get();
  assert(loaded.root == root);
  assert(loaded.default_aspect == "fill-height");
  assert(loaded.default_filter == "scanline");
  assert(loaded.system_language == "en");
  assert(!loaded.key_sound);
  assert(loaded.system_volume_percent == 35);
  assert(loaded.brightness_level == 6);
  assert(loaded.auto_sleep_minutes == 9);
  assert(!loaded.auto_sleep_enabled);
  assert(loaded.auto_sleep_interval_index == 4);
  assert(loaded.selected_avatar == "avatar.png");
  assert(!loaded.virtual_mouse);
  assert(loaded.mouse_speed == 960);
  assert(std::abs(loaded.mouse_accel - 2.0f) < 0.001f);
  assert(!store.IsDirty());

  store.Mutable().system_volume_percent = 40;
  store.MarkDirty(100);
  assert(!store.ShouldFlush(599, 500));
  assert(store.ShouldFlush(600, 500));
  assert(store.Save());
  assert(!store.IsDirty());

  const std::string round_trip = ReadAll(path);
  assert(round_trip.find("# user comment\n") != std::string::npos);
  assert(round_trip.find("future_engine_option=keep exactly\n") != std::string::npos);
  assert(round_trip.find("system_volume_percent=40\n") != std::string::npos);
  ConfigStore reloaded;
  reloaded.LoadFromPath(path);
  assert(reloaded.Get().system_volume_percent == 40);
  assert(reloaded.Get().default_filter == "scanline");

  {
    std::ofstream out(path, std::ios::trunc);
    out << "default_aspect=fit-width\n"
        << "auto_sleep_enabled=0\n"
        << "selected_avatar=legacy-avatar.png\n"
        << "system_volume_percent=999\n"
        << "brightness_level=-3\n"
        << "mouse_accel=0\n"
        << "third_party_key=preserved\n";
  }

  ConfigStore legacy;
  legacy.LoadFromPath(path);
  assert(legacy.Get().default_aspect == "contain");
  assert(!legacy.Get().auto_sleep_enabled);
  assert(legacy.Get().selected_avatar == "legacy-avatar.png");
  assert(legacy.Get().system_volume_percent == 100);
  assert(legacy.Get().brightness_level == 0);
  assert(std::abs(legacy.Get().mouse_accel - 0.1f) < 0.001f);
  assert(legacy.IsDirty());
  assert(legacy.ShouldFlush(0, 500));
  assert(legacy.Save());

  const std::string migrated = ReadAll(path);
  assert(migrated.find("default_aspect=contain\n") != std::string::npos);
  assert(migrated.find("lid_close_screen_off=0\n") != std::string::npos);
  assert(migrated.find("selected_contributor_avatar_label=legacy-avatar.png\n") != std::string::npos);
  assert(migrated.find("third_party_key=preserved\n") != std::string::npos);
  assert(migrated.find("auto_sleep_enabled=") == std::string::npos);
  assert(migrated.find("selected_avatar=") == std::string::npos);
  assert(Count(migrated, "default_aspect=") == 1);

  std::filesystem::remove_all(root, ec);
  std::cout << "config store tests passed\n";
  return 0;
}

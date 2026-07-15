#pragma once

#include "filesystem_compat.h"
#include <string>
#include <vector>

struct SystemControlValue {
  bool available = false;
  int level = 0;
  int max_level = 10;
};

struct SystemControlLevels {
  SystemControlValue volume;
  SystemControlValue brightness;
};

class SystemControlService {
public:
  explicit SystemControlService(bool prefer_system_volume);

  void Refresh(SystemControlLevels &levels);
  bool RefreshVolumeOnly(SystemControlValue &value);
  bool ApplyVolumePercent(int percent, SystemControlValue &value);
  bool ApplyBrightnessLevel(int level, SystemControlValue &value);
  bool AdjustVolume(int delta_steps, SystemControlLevels &levels);
  bool AdjustBrightness(int delta_steps, SystemControlLevels &levels);

private:
  struct BrightnessTarget {
    std::filesystem::path brightness_path;
    std::filesystem::path max_brightness_path;
    int max_brightness = 0;
  };

  static int ClampVolumeLevel(int level);
  static int ClampBrightnessLevel(int level);
  static int PercentToVolumeLevel(int percent);
  static int VolumeLevelToPercent(int level);
  static std::string TrimAscii(std::string text);
  static bool ParseBracketPercent(const std::string &text, int &out_percent);
  static bool ParseInt(const std::string &text, int &out_value);
  static bool ReadSmallIntFile(const std::filesystem::path &path, int &out_value);
  static bool WriteSmallIntFile(const std::filesystem::path &path, int value);
  static std::string RunCommandCapture(const std::string &command);
  static bool UseTrimuiShmvarVolume();
  static std::string ShellQuote(const std::string &text);

  bool TryReadVolumeLevelTrimuiShmvar(int &out_level);
  bool TrySetVolumeLevelTrimuiShmvar(int level);
  bool TryReadVolumePercentAlsa(int &out_percent);
  bool TrySetVolumePercentAlsa(int percent);
  bool TryReadBrightnessDisp(int &out_brightness) const;
  bool TrySetBrightnessDisp(int brightness) const;
  void RefreshVolume(SystemControlValue &value);
  void RefreshBrightness(SystemControlValue &value);
  bool SetVolumeLevel(int level, SystemControlValue &value);
  bool SetBrightnessLevel(int level, SystemControlValue &value);
  bool TryReadVolumePercent(const std::string &control, int &out_percent) const;
  bool TrySetVolumePercent(const std::string &control, int percent);
  void DiscoverBrightnessPaths();
  bool AddBrightnessTarget(const std::filesystem::path &brightness_path,
                           const std::filesystem::path &max_brightness_path,
                           int max_brightness,
                           const std::string &source);
  bool WriteBrightnessTargets(int requested_level, int &targets_written, int &targets_attempted);

  bool prefer_system_volume_ = false;
  bool volume_level_initialized_ = false;
  int cached_volume_level_ = 0;
  std::string working_volume_control_;
  std::string working_mixer_card_;
  std::string working_mixer_elem_;
  std::filesystem::path brightness_path_;
  std::filesystem::path brightness_max_path_;
  std::vector<BrightnessTarget> brightness_targets_;
  std::filesystem::path disp_device_path_ = "/dev/disp";
};

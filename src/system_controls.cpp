#include "system_controls.h"

#include <algorithm>
#include <array>
#include <cerrno>
#include <cctype>
#include <cstdio>
#include <cstdlib>
#include <fstream>
#include <iostream>
#include <sstream>
#include <vector>
#ifdef HAVE_ALSA
#include <alsa/asoundlib.h>
#endif
#if !defined(_WIN32)
#include <fcntl.h>
#include <sys/ioctl.h>
#include <unistd.h>
#endif

namespace {
constexpr int kDispBrightnessMax = 255;
constexpr int kDispBrightnessUsableMin = 20;
constexpr int kDispBrightnessUsableMax = 226;
constexpr int kBrightnessUiMaxLevel = 8;
constexpr int kSafeBrightnessMin = kDispBrightnessUsableMin;
constexpr int kSysfsBrightnessUsableMinRaw = 20;
#if !defined(_WIN32)
constexpr unsigned long kDispSetBrightness = 0x102;
constexpr unsigned long kDispGetBrightness = 0x103;
#endif

constexpr std::array<const char *, 6> kMixerControls = {
    "Master",
    "Speaker",
    "Playback",
    "PCM",
    "Headphone",
    "DAC",
};

#ifdef HAVE_ALSA
std::string ToLowerAscii(std::string text) {
  std::transform(text.begin(), text.end(), text.begin(),
                 [](unsigned char ch) { return static_cast<char>(std::tolower(ch)); });
  return text;
}

int MixerElementScore(const std::string &name, const std::string &cached_name) {
  const std::string lower = ToLowerAscii(name);
  if (!cached_name.empty() && name == cached_name) return 1000;
  if (lower.find("lineout") != std::string::npos || lower.find("line out") != std::string::npos) return 100;
  if (lower.find("headphone") != std::string::npos || lower.find("hp") != std::string::npos) return 90;
  if (lower.find("speaker") != std::string::npos || lower.find("spk") != std::string::npos) return 80;
  if (lower.find("master") != std::string::npos) return 70;
  if (lower.find("playback") != std::string::npos) return 60;
  if (lower == "pcm" || lower.find(" pcm") != std::string::npos) return 50;
  if (lower.find("dac") != std::string::npos) return 40;
  if (lower.find("digital") != std::string::npos) return -100;
  return 0;
}
#endif

std::string ReadSmallTextFile(const std::filesystem::path &path) {
  std::ifstream in(path);
  if (!in) return {};
  std::ostringstream oss;
  oss << in.rdbuf();
  return oss.str();
}

void LogSystemControl(const std::string &message) {
  auto enabled = [](const char *value) {
    return value && *value && std::string(value) != "0";
  };
  if (!enabled(std::getenv("ROCREADER_SYSTEM_CONTROL_LOG")) &&
      !enabled(std::getenv("ROCREADER_VERBOSE_LOG")) &&
      !enabled(std::getenv("ROCREADER_DEBUG_LOG"))) {
    return;
  }
  std::cout << "[native_h700][system_controls] " << message << "\n";
}

#ifdef HAVE_ALSA
std::vector<std::string> UniqueMixerCards(const std::string &preferred) {
  std::vector<std::string> cards;
  auto add = [&](const std::string &card) {
    if (card.empty()) return;
    if (std::find(cards.begin(), cards.end(), card) == cards.end()) cards.push_back(card);
  };
  add(preferred);
  add("default");
  add("hw:0");
  add("sysdefault");
  add("pulse");
  return cards;
}
#endif

int DispRawBrightnessToLevel(int raw_brightness) {
  const int clamped = std::clamp(raw_brightness, kDispBrightnessUsableMin, kDispBrightnessUsableMax);
  const int span = std::max(1, kDispBrightnessUsableMax - kDispBrightnessUsableMin);
  return std::clamp(((clamped - kDispBrightnessUsableMin) * kBrightnessUiMaxLevel + span / 2) / span,
                    0, kBrightnessUiMaxLevel);
}

int LevelToDispRawBrightness(int level) {
  const int clamped = std::clamp(level, 0, kBrightnessUiMaxLevel);
  const int span = std::max(1, kDispBrightnessUsableMax - kDispBrightnessUsableMin);
  return std::clamp(kDispBrightnessUsableMin +
                        (clamped * span + kBrightnessUiMaxLevel / 2) / kBrightnessUiMaxLevel,
                    kDispBrightnessUsableMin, kDispBrightnessUsableMax);
}

int SysfsRawBrightnessToLevel(int raw_brightness, int max_brightness) {
  if (max_brightness <= 0) return 0;
  const int min_raw = max_brightness <= kSysfsBrightnessUsableMinRaw ? 1 : kSysfsBrightnessUsableMinRaw;
  if (max_brightness <= min_raw) return raw_brightness > 0 ? kBrightnessUiMaxLevel : 0;
  const int clamped = std::clamp(raw_brightness, min_raw, max_brightness);
  const int span = max_brightness - min_raw;
  return std::clamp(((clamped - min_raw) * kBrightnessUiMaxLevel + span / 2) / span,
                    0, kBrightnessUiMaxLevel);
}

int LevelToSysfsRawBrightness(int level, int max_brightness) {
  if (max_brightness <= 0) return 0;
  const int clamped = std::clamp(level, 0, kBrightnessUiMaxLevel);
  const int min_raw = max_brightness <= kSysfsBrightnessUsableMinRaw ? 1 : kSysfsBrightnessUsableMinRaw;
  if (max_brightness <= min_raw) return max_brightness;
  const int span = max_brightness - min_raw;
  return std::clamp(min_raw + (clamped * span + kBrightnessUiMaxLevel / 2) / kBrightnessUiMaxLevel,
                    min_raw, max_brightness);
}

int ReadEnvInt(const char *name, int fallback_value, int min_value, int max_value) {
  const char *raw = std::getenv(name);
  if (!raw || !*raw) return fallback_value;
  try {
    return std::clamp(std::stoi(raw), min_value, max_value);
  } catch (...) {
    return fallback_value;
  }
}

int VolumeUiMaxLevel() {
  return ReadEnvInt("ROCREADER_SYSTEM_VOLUME_LEVELS", 10, 1, 100);
}

int VolumeOutputMaxPercent() {
  return ReadEnvInt("ROCREADER_SYSTEM_VOLUME_OUTPUT_MAX_PERCENT", 100, 1, 100);
}

int OutputPercentToUiPercent(int output_percent) {
  const int max_output = VolumeOutputMaxPercent();
  return std::clamp((std::clamp(output_percent, 0, max_output) * 100 + max_output / 2) / max_output, 0, 100);
}

int UiPercentToOutputPercent(int ui_percent) {
  const int max_output = VolumeOutputMaxPercent();
  return std::clamp((std::clamp(ui_percent, 0, 100) * max_output + 50) / 100, 0, max_output);
}

std::string ReplaceAll(std::string text, const std::string &from, const std::string &to) {
  if (from.empty()) return text;
  size_t pos = 0;
  while ((pos = text.find(from, pos)) != std::string::npos) {
    text.replace(pos, from.size(), to);
    pos += to.size();
  }
  return text;
}

bool g_shared_volume_level_initialized = false;
int g_shared_volume_level = 0;

void StoreSharedVolumeLevel(int level) {
  g_shared_volume_level = level;
  g_shared_volume_level_initialized = true;
}

bool TryLoadSharedVolumeLevel(int &level) {
  if (!g_shared_volume_level_initialized) return false;
  level = g_shared_volume_level;
  return true;
}
} // namespace

SystemControlService::SystemControlService(bool prefer_system_volume)
    : prefer_system_volume_(prefer_system_volume) {
  if (const char *env_disp = std::getenv("ROCREADER_DISP_DEVICE"); env_disp && *env_disp) {
    disp_device_path_ = env_disp;
  }
  DiscoverBrightnessPaths();
  LogSystemControl("init prefer_system_volume=" + std::string(prefer_system_volume_ ? "1" : "0") +
                   " brightness_path=" + brightness_path_.string() +
                   " brightness_max_path=" + brightness_max_path_.string() +
                   " disp_device_path=" + disp_device_path_.string());
}

void SystemControlService::Refresh(SystemControlLevels &levels) {
  RefreshVolume(levels.volume);
  RefreshBrightness(levels.brightness);
}

bool SystemControlService::RefreshVolumeOnly(SystemControlValue &value) {
  RefreshVolume(value);
  return value.available;
}

bool SystemControlService::ApplyVolumePercent(int percent, SystemControlValue &value) {
  const int ui_percent = std::clamp(percent, 0, 100);
  const int max_level = VolumeUiMaxLevel();
  return SetVolumeLevel(ClampVolumeLevel((ui_percent * max_level + 50) / 100), value);
}

bool SystemControlService::ApplyBrightnessLevel(int level, SystemControlValue &value) {
  return SetBrightnessLevel(ClampBrightnessLevel(level), value);
}

bool SystemControlService::AdjustVolume(int delta_steps, SystemControlLevels &levels) {
  levels.volume.max_level = VolumeUiMaxLevel();
  if (volume_level_initialized_) {
    levels.volume.available = true;
    levels.volume.level = ClampVolumeLevel(cached_volume_level_);
  } else {
    RefreshVolume(levels.volume);
    if (!levels.volume.available) return false;
  }
  const int next_level = ClampVolumeLevel(levels.volume.level + delta_steps);
  return SetVolumeLevel(next_level, levels.volume);
}

bool SystemControlService::AdjustBrightness(int delta_steps, SystemControlLevels &levels) {
  RefreshBrightness(levels.brightness);
  if (!levels.brightness.available) return false;
  const int next_level = ClampBrightnessLevel(levels.brightness.level + delta_steps);
  return SetBrightnessLevel(next_level, levels.brightness);
}

int SystemControlService::ClampVolumeLevel(int level) { return std::clamp(level, 0, VolumeUiMaxLevel()); }

int SystemControlService::ClampBrightnessLevel(int level) { return std::clamp(level, 0, kBrightnessUiMaxLevel); }

int SystemControlService::PercentToVolumeLevel(int percent) {
  const int ui_percent = OutputPercentToUiPercent(percent);
  const int max_level = VolumeUiMaxLevel();
  return ClampVolumeLevel((ui_percent * max_level + 50) / 100);
}

int SystemControlService::VolumeLevelToPercent(int level) {
  const int max_level = VolumeUiMaxLevel();
  return std::clamp((ClampVolumeLevel(level) * 100 + max_level / 2) / max_level, 0, 100);
}

std::string SystemControlService::TrimAscii(std::string text) {
  while (!text.empty() && std::isspace(static_cast<unsigned char>(text.front())) != 0) {
    text.erase(text.begin());
  }
  while (!text.empty() && std::isspace(static_cast<unsigned char>(text.back())) != 0) {
    text.pop_back();
  }
  return text;
}

bool SystemControlService::ParseBracketPercent(const std::string &text, int &out_percent) {
  size_t pos = 0;
  while (true) {
    pos = text.find('[', pos);
    if (pos == std::string::npos) return false;
    const size_t percent_pos = text.find('%', pos + 1);
    if (percent_pos == std::string::npos) return false;
    bool all_digits = percent_pos > pos + 1;
    for (size_t i = pos + 1; i < percent_pos; ++i) {
      if (!std::isdigit(static_cast<unsigned char>(text[i]))) {
        all_digits = false;
        break;
      }
    }
    if (all_digits) {
      try {
        out_percent = std::stoi(text.substr(pos + 1, percent_pos - pos - 1));
        return true;
      } catch (...) {
        return false;
      }
    }
    pos = percent_pos + 1;
  }
}

bool SystemControlService::ParseInt(const std::string &text, int &out_value) {
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

bool SystemControlService::ReadSmallIntFile(const std::filesystem::path &path, int &out_value) {
  if (path.empty()) return false;
  return ParseInt(TrimAscii(ReadSmallTextFile(path)), out_value);
}

bool SystemControlService::WriteSmallIntFile(const std::filesystem::path &path, int value) {
  if (path.empty()) return false;
  std::ofstream out(path, std::ios::trunc);
  if (!out) return false;
  out << value;
  return static_cast<bool>(out);
}

bool SystemControlService::TryReadVolumePercentAlsa(int &out_percent) {
#ifdef HAVE_ALSA
  for (const std::string &card : UniqueMixerCards(working_mixer_card_)) {
    snd_mixer_t *handle = nullptr;
    if (snd_mixer_open(&handle, 0) < 0 || !handle) continue;
    auto close_handle = [&]() {
      if (handle) snd_mixer_close(handle);
      handle = nullptr;
    };
    if (snd_mixer_attach(handle, card.c_str()) < 0 ||
        snd_mixer_selem_register(handle, nullptr, nullptr) < 0 ||
        snd_mixer_load(handle) < 0) {
      close_handle();
      continue;
    }

    snd_mixer_elem_t *best_elem = nullptr;
    std::string best_elem_name;
    int best_score = -10000;
    long best_minv = 0;
    long best_maxv = 0;
    for (snd_mixer_elem_t *elem = snd_mixer_first_elem(handle); elem; elem = snd_mixer_elem_next(elem)) {
      if (!snd_mixer_selem_is_active(elem) || !snd_mixer_selem_has_playback_volume(elem)) continue;
      const char *elem_name = snd_mixer_selem_get_name(elem);
      const std::string elem_name_text = elem_name ? elem_name : "";
      long minv = 0;
      long maxv = 0;
      if (snd_mixer_selem_get_playback_volume_range(elem, &minv, &maxv) < 0 || maxv <= minv) continue;
      const int score = MixerElementScore(elem_name_text, working_mixer_elem_);
      if (score < best_score) continue;
      best_score = score;
      best_elem = elem;
      best_elem_name = elem_name_text;
      best_minv = minv;
      best_maxv = maxv;
    }

    if (best_elem) {
      long sum = 0;
      int count = 0;
      for (int ch = 0; ch <= SND_MIXER_SCHN_LAST; ++ch) {
        const auto channel = static_cast<snd_mixer_selem_channel_id_t>(ch);
        if (!snd_mixer_selem_has_playback_channel(best_elem, channel)) continue;
        long raw = 0;
        if (snd_mixer_selem_get_playback_volume(best_elem, channel, &raw) == 0) {
          sum += raw;
          ++count;
        }
      }
      if (count > 0) {
        const long avg = sum / count;
        out_percent =
            std::clamp(static_cast<int>(((avg - best_minv) * 100 + (best_maxv - best_minv) / 2) /
                                        (best_maxv - best_minv)),
                       0, 100);
        working_mixer_card_ = card;
        working_mixer_elem_ = best_elem_name;
        close_handle();
        LogSystemControl("read volume percent success via ALSA: card=" + working_mixer_card_ +
                         " elem=" + working_mixer_elem_ +
                         " score=" + std::to_string(best_score) +
                         " percent=" + std::to_string(out_percent));
        return true;
      }
    }
    close_handle();
  }
  LogSystemControl("read volume percent via ALSA failed");
#else
  LogSystemControl("read volume percent via ALSA skipped: HAVE_ALSA not enabled");
#endif
  return false;
}

bool SystemControlService::TrySetVolumePercentAlsa(int percent) {
#ifdef HAVE_ALSA
  const int clamped_percent = std::clamp(percent, 0, 100);
  bool changed_any = false;
  std::string changed_names;
  for (const std::string &card : UniqueMixerCards(working_mixer_card_)) {
    snd_mixer_t *handle = nullptr;
    if (snd_mixer_open(&handle, 0) < 0 || !handle) continue;
    auto close_handle = [&]() {
      if (handle) snd_mixer_close(handle);
      handle = nullptr;
    };
    if (snd_mixer_attach(handle, card.c_str()) < 0 ||
        snd_mixer_selem_register(handle, nullptr, nullptr) < 0 ||
        snd_mixer_load(handle) < 0) {
      close_handle();
      continue;
    }

    for (snd_mixer_elem_t *elem = snd_mixer_first_elem(handle); elem; elem = snd_mixer_elem_next(elem)) {
      if (!snd_mixer_selem_is_active(elem) || !snd_mixer_selem_has_playback_volume(elem)) continue;
      const char *elem_name_c = snd_mixer_selem_get_name(elem);
      const std::string elem_name = elem_name_c ? elem_name_c : "";
      const int score = MixerElementScore(elem_name, working_mixer_elem_);
      if (score < 0) continue;

      long minv = 0;
      long maxv = 0;
      if (snd_mixer_selem_get_playback_volume_range(elem, &minv, &maxv) < 0 || maxv <= minv) continue;
      const long raw = minv + ((maxv - minv) * clamped_percent + 50) / 100;
      if (snd_mixer_selem_set_playback_volume_all(elem, raw) < 0) continue;
      if (snd_mixer_selem_has_playback_switch(elem)) {
        snd_mixer_selem_set_playback_switch_all(elem, clamped_percent > 0 ? 1 : 0);
      }
      if (!changed_any || score > MixerElementScore(working_mixer_elem_, "")) {
        working_mixer_card_ = card;
        working_mixer_elem_ = elem_name;
      }
      changed_any = true;
      if (!changed_names.empty()) changed_names += ",";
      changed_names += elem_name;
    }
    close_handle();
    if (changed_any) {
      LogSystemControl("set volume percent success via ALSA: card=" + working_mixer_card_ +
                       " elem=" + working_mixer_elem_ +
                       " changed=" + changed_names +
                       " percent=" + std::to_string(clamped_percent));
      return true;
    }
  }
  LogSystemControl("set volume percent via ALSA failed: percent=" + std::to_string(clamped_percent));
#else
  LogSystemControl("set volume percent via ALSA skipped: HAVE_ALSA not enabled");
#endif
  return false;
}

bool SystemControlService::TryReadBrightnessDisp(int &out_brightness) const {
#if !defined(_WIN32)
  const int fd = open(disp_device_path_.string().c_str(), O_RDWR);
  if (fd < 0) {
    LogSystemControl("read brightness via disp failed: open path=" + disp_device_path_.string() +
                     " errno=" + std::to_string(errno));
    return false;
  }
  unsigned long args[4] = {0, 0, 0, 0};
  const int rc = ioctl(fd, kDispGetBrightness, args);
  close(fd);
  if (rc < 0) {
    LogSystemControl("read brightness via disp failed: ioctl path=" + disp_device_path_.string() +
                     " errno=" + std::to_string(errno));
    return false;
  }
  out_brightness = std::clamp(rc, 0, kDispBrightnessMax);
  LogSystemControl("read brightness via disp success: raw_brightness=" + std::to_string(out_brightness) +
                   " ioctl_rc=" + std::to_string(rc));
  return true;
#else
  return false;
#endif
}

bool SystemControlService::TrySetBrightnessDisp(int brightness) const {
#if !defined(_WIN32)
  const int raw = std::clamp(brightness, 0, kDispBrightnessMax);
  const int fd = open(disp_device_path_.string().c_str(), O_RDWR);
  if (fd < 0) {
    LogSystemControl("set brightness via disp failed: open path=" + disp_device_path_.string() +
                     " raw_brightness=" + std::to_string(raw) +
                     " errno=" + std::to_string(errno));
    return false;
  }
  unsigned long args[4] = {0, static_cast<unsigned long>(raw), 0, 0};
  const int rc = ioctl(fd, kDispSetBrightness, args);
  close(fd);
  if (rc < 0) {
    LogSystemControl("set brightness via disp failed: ioctl path=" + disp_device_path_.string() +
                     " raw_brightness=" + std::to_string(raw) +
                     " errno=" + std::to_string(errno));
    return false;
  }
  LogSystemControl("set brightness via disp success: raw_brightness=" + std::to_string(raw) +
                   " ioctl_rc=" + std::to_string(rc));
  return true;
#else
  return false;
#endif
}

std::string SystemControlService::RunCommandCapture(const std::string &command) {
#if defined(_WIN32)
  FILE *pipe = _popen(command.c_str(), "r");
#else
  FILE *pipe = popen(command.c_str(), "r");
#endif
  if (!pipe) return {};

  std::string output;
  char buffer[256];
  while (std::fgets(buffer, static_cast<int>(sizeof(buffer)), pipe) != nullptr) {
    output += buffer;
  }

#if defined(_WIN32)
  _pclose(pipe);
#else
  pclose(pipe);
#endif
  return output;
}

bool SystemControlService::UseTrimuiShmvarVolume() {
  const char *env = std::getenv("ROCREADER_TRIMUI_SHMVAR_VOLUME");
  return env && *env && std::string(env) != "0";
}

std::string SystemControlService::ShellQuote(const std::string &text) {
  std::string quoted = "'";
  for (char ch : text) {
    if (ch == '\'') {
      quoted += "'\\''";
    } else {
      quoted += ch;
    }
  }
  quoted += "'";
  return quoted;
}

bool SystemControlService::TryReadVolumeLevelTrimuiShmvar(int &out_level) {
  if (!UseTrimuiShmvarVolume()) return false;
  const char *path_env = std::getenv("ROCREADER_TRIMUI_SHMVAR_PATH");
  const std::string path = (path_env && *path_env) ? path_env : "/usr/trimui/bin/shmvar";
  const std::string output = RunCommandCapture(ShellQuote(path) + " vol 2>/dev/null");
  int raw_level = 0;
  if (!ParseInt(TrimAscii(output), raw_level)) {
    LogSystemControl("read volume level via trimui shmvar failed: output=" + TrimAscii(output));
    return false;
  }
  out_level = ClampVolumeLevel(raw_level);
  LogSystemControl("read volume level success via trimui shmvar: raw_level=" + std::to_string(raw_level) +
                   " level=" + std::to_string(out_level) +
                   " max=" + std::to_string(VolumeUiMaxLevel()));
  return true;
}

bool SystemControlService::TrySetVolumeLevelTrimuiShmvar(int level) {
  if (!UseTrimuiShmvarVolume()) return false;
  const int target_level = ClampVolumeLevel(level);
  const char *path_env = std::getenv("ROCREADER_TRIMUI_SHMVAR_PATH");
  const std::string path = (path_env && *path_env) ? path_env : "/usr/trimui/bin/shmvar";
  const std::string quoted_path = ShellQuote(path);
  std::vector<std::string> commands;
  if (const char *template_env = std::getenv("ROCREADER_TRIMUI_SHMVAR_VOLUME_SET_COMMAND");
      template_env && *template_env) {
    std::string command = template_env;
    command = ReplaceAll(command, "{level}", std::to_string(target_level));
    command = ReplaceAll(command, "%d", std::to_string(target_level));
    commands.push_back(command);
  }
  commands.push_back(quoted_path + " vol " + std::to_string(target_level));
  commands.push_back(quoted_path + " vol=" + std::to_string(target_level));
  commands.push_back(quoted_path + " set vol " + std::to_string(target_level));

  for (const std::string &command : commands) {
    const int rc = std::system((command + " >/dev/null 2>&1").c_str());
    int read_back = -1;
    if (TryReadVolumeLevelTrimuiShmvar(read_back) && read_back == target_level) {
      LogSystemControl("set volume level success via trimui shmvar: level=" + std::to_string(target_level) +
                       " rc=" + std::to_string(rc) +
                       " command=" + command);
      return true;
    }
    LogSystemControl("set volume level via trimui shmvar candidate failed: level=" + std::to_string(target_level) +
                     " rc=" + std::to_string(rc) +
                     " read_back=" + std::to_string(read_back) +
                     " command=" + command);
  }
  return false;
}

void SystemControlService::RefreshVolume(SystemControlValue &value) {
  value = {};
  value.max_level = VolumeUiMaxLevel();
  if (!prefer_system_volume_) {
    LogSystemControl("refresh volume skipped: prefer_system_volume=0");
    return;
  }

  int shared_volume_level = 0;
  if (TryLoadSharedVolumeLevel(shared_volume_level)) {
    value.available = true;
    value.level = ClampVolumeLevel(shared_volume_level);
    LogSystemControl("refresh volume success via shared cached level: level=" + std::to_string(value.level) +
                     " max=" + std::to_string(value.max_level));
    return;
  }

  int shmvar_level = -1;
  if (TryReadVolumeLevelTrimuiShmvar(shmvar_level)) {
    value.available = true;
    value.level = shmvar_level;
    LogSystemControl("refresh volume success via trimui shmvar: level=" + std::to_string(value.level));
    return;
  }

  int percent = -1;
  if (TryReadVolumePercentAlsa(percent)) {
    value.available = true;
    value.level = PercentToVolumeLevel(percent);
    LogSystemControl("refresh volume success via ALSA: percent=" + std::to_string(percent) +
                     " level=" + std::to_string(value.level));
    return;
  }

  if (!working_volume_control_.empty() && TryReadVolumePercent(working_volume_control_, percent)) {
    value.available = true;
    value.level = PercentToVolumeLevel(percent);
    LogSystemControl("refresh volume success: control=" + working_volume_control_ +
                     " percent=" + std::to_string(percent) +
                     " level=" + std::to_string(value.level));
    return;
  }

  for (const char *control : kMixerControls) {
    if (!TryReadVolumePercent(control, percent)) continue;
    working_volume_control_ = control;
    value.available = true;
    value.level = PercentToVolumeLevel(percent);
    LogSystemControl("refresh volume discovered control=" + working_volume_control_ +
                     " percent=" + std::to_string(percent) +
                     " level=" + std::to_string(value.level));
    return;
  }

  LogSystemControl("refresh volume failed: no usable mixer control found");
  if (volume_level_initialized_) {
    value.available = true;
    value.level = ClampVolumeLevel(cached_volume_level_);
    LogSystemControl("refresh volume fallback via cached level: level=" + std::to_string(value.level) +
                     " max=" + std::to_string(value.max_level));
  }
}

void SystemControlService::RefreshBrightness(SystemControlValue &value) {
  value = {};
  value.max_level = kBrightnessUiMaxLevel;
  DiscoverBrightnessPaths();
  int raw_brightness = 0;
  int max_brightness = 0;
  if (TryReadBrightnessDisp(raw_brightness)) {
    value.available = true;
    const int clamped_raw = std::clamp(raw_brightness, kDispBrightnessUsableMin, kDispBrightnessUsableMax);
    value.level = ClampBrightnessLevel(DispRawBrightnessToLevel(clamped_raw));
    LogSystemControl("refresh brightness success via disp: raw_brightness=" + std::to_string(raw_brightness) +
                     " clamped_raw=" + std::to_string(clamped_raw) +
                     " usable_max=" + std::to_string(kDispBrightnessUsableMax) +
                     " level=" + std::to_string(value.level));
    return;
  }

  if (!ReadSmallIntFile(brightness_path_, raw_brightness) || !ReadSmallIntFile(brightness_max_path_, max_brightness) ||
      max_brightness <= 0) {
    LogSystemControl("refresh brightness failed: brightness_path=" + brightness_path_.string() +
                     " brightness_max_path=" + brightness_max_path_.string() +
                     " raw_brightness=" + std::to_string(raw_brightness) +
                     " max_brightness=" + std::to_string(max_brightness));
    return;
  }
  value.available = true;
  value.level = ClampBrightnessLevel(SysfsRawBrightnessToLevel(raw_brightness, max_brightness));
  LogSystemControl("refresh brightness success: brightness_path=" + brightness_path_.string() +
                   " raw_brightness=" + std::to_string(raw_brightness) +
                   " max_brightness=" + std::to_string(max_brightness) +
                   " level=" + std::to_string(value.level));
}

bool SystemControlService::SetVolumeLevel(int level, SystemControlValue &value) {
  value.max_level = VolumeUiMaxLevel();
  if (!prefer_system_volume_) {
    LogSystemControl("set volume skipped: prefer_system_volume=0 requested_level=" + std::to_string(level));
    return false;
  }
  const int ui_percent = VolumeLevelToPercent(level);
  const int percent = UiPercentToOutputPercent(ui_percent);
  LogSystemControl("set volume request: level=" + std::to_string(level) +
                   " ui_percent=" + std::to_string(ui_percent) +
                   " output_percent=" + std::to_string(percent) +
                   " output_max_percent=" + std::to_string(VolumeOutputMaxPercent()) +
                   " working_control=" + working_volume_control_);

  if (TrySetVolumeLevelTrimuiShmvar(level)) {
    RefreshVolume(value);
    if (value.available) {
      cached_volume_level_ = value.level;
      volume_level_initialized_ = true;
      StoreSharedVolumeLevel(value.level);
      LogSystemControl("set volume success via trimui shmvar: resulting_level=" + std::to_string(value.level));
      return true;
    }
  }

  if (TrySetVolumePercentAlsa(percent)) {
    value.available = true;
    value.max_level = VolumeUiMaxLevel();
    value.level = ClampVolumeLevel(level);
    cached_volume_level_ = value.level;
    volume_level_initialized_ = true;
    StoreSharedVolumeLevel(value.level);
    LogSystemControl("set volume success via ALSA: resulting_level=" + std::to_string(value.level));
    return true;
  }

  if (!working_volume_control_.empty() && TrySetVolumePercent(working_volume_control_, percent)) {
    value.available = true;
    value.max_level = VolumeUiMaxLevel();
    value.level = ClampVolumeLevel(level);
    cached_volume_level_ = value.level;
    volume_level_initialized_ = true;
    StoreSharedVolumeLevel(value.level);
    LogSystemControl("set volume success via cached control: control=" + working_volume_control_ +
                     " resulting_level=" + std::to_string(value.level));
    return true;
  }

  for (const char *control : kMixerControls) {
    if (!TrySetVolumePercent(control, percent)) continue;
    working_volume_control_ = control;
    value.available = true;
    value.max_level = VolumeUiMaxLevel();
    value.level = ClampVolumeLevel(level);
    cached_volume_level_ = value.level;
    volume_level_initialized_ = true;
    StoreSharedVolumeLevel(value.level);
    LogSystemControl("set volume success via discovered control: control=" + working_volume_control_ +
                     " resulting_level=" + std::to_string(value.level));
    return true;
  }
  LogSystemControl("set volume failed: level=" + std::to_string(level) +
                   " percent=" + std::to_string(percent));
  return false;
}

bool SystemControlService::SetBrightnessLevel(int level, SystemControlValue &value) {
  value.max_level = kBrightnessUiMaxLevel;
  DiscoverBrightnessPaths();
  const int requested_level = ClampBrightnessLevel(level);
  const int disp_raw_value = LevelToDispRawBrightness(requested_level);
  if (TrySetBrightnessDisp(disp_raw_value)) {
    RefreshBrightness(value);
    if (value.available && value.level != requested_level) {
      const bool increasing = value.level < requested_level;
      const int start = disp_raw_value + (increasing ? 1 : -1);
      const int end = increasing ? kDispBrightnessUsableMax : kSafeBrightnessMin;
      const int step = increasing ? 1 : -1;
      for (int probe_raw = start; increasing ? (probe_raw <= end) : (probe_raw >= end); probe_raw += step) {
        if (!TrySetBrightnessDisp(probe_raw)) continue;
        RefreshBrightness(value);
        LogSystemControl("set brightness probe via disp: requested_level=" + std::to_string(requested_level) +
                         " probe_raw=" + std::to_string(probe_raw) +
                         " resulting_level=" + std::to_string(value.level));
        if (!value.available) continue;
        if ((increasing && value.level >= requested_level) || (!increasing && value.level <= requested_level)) {
          break;
        }
      }
    }
    LogSystemControl("set brightness success via disp: requested_level=" + std::to_string(level) +
                     " raw_value=" + std::to_string(disp_raw_value) +
                     " resulting_level=" + std::to_string(value.level));
    return value.available;
  }

  int targets_written = 0;
  int targets_attempted = 0;
  if (!WriteBrightnessTargets(requested_level, targets_written, targets_attempted)) {
    LogSystemControl("set brightness failed: requested_level=" + std::to_string(level) +
                     " targets_attempted=" + std::to_string(targets_attempted) +
                     " targets_written=" + std::to_string(targets_written));
    return false;
  }
  RefreshBrightness(value);
  LogSystemControl("set brightness success: requested_level=" + std::to_string(level) +
                   " targets_attempted=" + std::to_string(targets_attempted) +
                   " targets_written=" + std::to_string(targets_written) +
                   " resulting_level=" + std::to_string(value.level));
  return value.available;
}

bool SystemControlService::TryReadVolumePercent(const std::string &control, int &out_percent) const {
  const std::string escaped_control = "'" + control + "'";
  const std::array<std::string, 4> commands = {
      "amixer -q sget " + escaped_control + " 2>/dev/null",
      "/usr/bin/amixer -q sget " + escaped_control + " 2>/dev/null",
      "amixer -q -c 0 sget " + escaped_control + " 2>/dev/null",
      "/usr/bin/amixer -q -c 0 sget " + escaped_control + " 2>/dev/null",
  };
  for (const std::string &command : commands) {
    const std::string output = RunCommandCapture(command);
    if (output.empty()) continue;
    if (ParseBracketPercent(output, out_percent)) {
      LogSystemControl("read volume percent success: control=" + control +
                       " command=" + command +
                       " percent=" + std::to_string(out_percent));
      return true;
    }
  }
  LogSystemControl("read volume percent failed: control=" + control);
  return false;
}

bool SystemControlService::TrySetVolumePercent(const std::string &control, int percent) {
  const std::string escaped_control = "'" + control + "'";
  const std::string suffix =
      std::to_string(std::clamp(percent, 0, 100)) + "%" + (percent <= 0 ? " mute" : " unmute") + " >/dev/null 2>&1";
  const std::array<std::string, 4> commands = {
      "amixer -q sset " + escaped_control + " " + suffix,
      "/usr/bin/amixer -q sset " + escaped_control + " " + suffix,
      "amixer -q -c 0 sset " + escaped_control + " " + suffix,
      "/usr/bin/amixer -q -c 0 sset " + escaped_control + " " + suffix,
  };
  for (const std::string &command : commands) {
    const int rc = std::system(command.c_str());
    if (rc == 0) {
      LogSystemControl("set volume percent success: control=" + control +
                       " percent=" + std::to_string(percent) +
                       " command=" + command);
      return true;
    }
  }
  LogSystemControl("set volume percent failed: control=" + control +
                   " percent=" + std::to_string(percent));
  return false;
}

void SystemControlService::DiscoverBrightnessPaths() {
  if (!brightness_targets_.empty()) return;

  const char *env_brightness = std::getenv("ROCREADER_BRIGHTNESS_PATH");
  const char *env_brightness_max = std::getenv("ROCREADER_BRIGHTNESS_MAX_PATH");
  if (env_brightness && *env_brightness && env_brightness_max && *env_brightness_max) {
    int env_max_brightness = 0;
    if (ReadSmallIntFile(env_brightness_max, env_max_brightness) && env_max_brightness > 0 &&
        AddBrightnessTarget(env_brightness, env_brightness_max, env_max_brightness, "env")) {
      LogSystemControl("discover brightness paths from env complete: targets=" +
                       std::to_string(brightness_targets_.size()));
      return;
    }
    LogSystemControl("discover brightness paths from env failed: brightness_path=" + std::string(env_brightness) +
                     " brightness_max_path=" + std::string(env_brightness_max) +
                     " max_brightness=" + std::to_string(env_max_brightness));
  } else if ((env_brightness && *env_brightness) || (env_brightness_max && *env_brightness_max)) {
    LogSystemControl("discover brightness env ignored: both ROCREADER_BRIGHTNESS_PATH and "
                     "ROCREADER_BRIGHTNESS_MAX_PATH are required for explicit override");
  }

  const char *env_targets = std::getenv("ROCREADER_BRIGHTNESS_TARGETS");
  if (env_targets && *env_targets) {
    std::stringstream ss(env_targets);
    std::string item;
    while (std::getline(ss, item, ':')) {
      const std::string trimmed = TrimAscii(item);
      if (trimmed.empty()) continue;
      const std::filesystem::path brightness = trimmed;
      const std::filesystem::path max_brightness = brightness.parent_path() / "max_brightness";
      int target_max = 0;
      if (!ReadSmallIntFile(max_brightness, target_max) || target_max <= 0) {
        LogSystemControl("discover brightness target skipped from env list: brightness_path=" +
                         brightness.string() +
                         " max_brightness_path=" + max_brightness.string() +
                         " max_brightness=" + std::to_string(target_max));
        continue;
      }
      AddBrightnessTarget(brightness, max_brightness, target_max, "env-list");
    }
    if (!brightness_targets_.empty()) {
      LogSystemControl("discover brightness paths from env list complete: targets=" +
                       std::to_string(brightness_targets_.size()));
      return;
    }
  }

  const char *env_root = std::getenv("ROCREADER_BRIGHTNESS_ROOT");
  const std::filesystem::path root =
      (env_root && *env_root) ? std::filesystem::path(env_root) : std::filesystem::path("/sys/class/backlight");
  std::error_code ec;
  if (!std::filesystem::exists(root, ec) || ec) {
    LogSystemControl("discover brightness paths failed: root missing path=" + root.string());
    return;
  }

  for (std::filesystem::directory_iterator it(root, ec), end; it != end; it.increment(ec)) {
    if (ec) {
      ec.clear();
      continue;
    }
    const std::filesystem::path dir = it->path();
    std::error_code dir_ec;
    if (!std::filesystem::is_directory(dir, dir_ec) || dir_ec) {
      continue;
    }
    const std::filesystem::path brightness = dir / "brightness";
    const std::filesystem::path max_brightness = dir / "max_brightness";
    std::error_code file_ec;
    if (!std::filesystem::exists(brightness, file_ec) || file_ec) {
      LogSystemControl("discover brightness candidate skipped: missing brightness path=" + brightness.string());
      continue;
    }
    file_ec.clear();
    if (!std::filesystem::exists(max_brightness, file_ec) || file_ec) {
      LogSystemControl("discover brightness candidate skipped: missing max_brightness path=" +
                       max_brightness.string());
      continue;
    }
    int target_max = 0;
    if (!ReadSmallIntFile(max_brightness, target_max) || target_max <= 0) {
      LogSystemControl("discover brightness candidate skipped: invalid max_brightness path=" +
                       max_brightness.string() +
                       " max_brightness=" + std::to_string(target_max));
      continue;
    }
    AddBrightnessTarget(brightness, max_brightness, target_max, "sysfs");
  }

  if (!brightness_targets_.empty()) {
    LogSystemControl("discover brightness paths success: primary_brightness_path=" + brightness_path_.string() +
                     " primary_brightness_max_path=" + brightness_max_path_.string() +
                     " targets=" + std::to_string(brightness_targets_.size()));
    return;
  }

  LogSystemControl("discover brightness paths incomplete: root=" + root.string() +
                   " primary_brightness_path=" + brightness_path_.string() +
                   " primary_brightness_max_path=" + brightness_max_path_.string());
}

bool SystemControlService::AddBrightnessTarget(const std::filesystem::path &brightness_path,
                                               const std::filesystem::path &max_brightness_path,
                                               int max_brightness,
                                               const std::string &source) {
  if (brightness_path.empty() || max_brightness_path.empty() || max_brightness <= 0) return false;
  const std::string brightness_text = brightness_path.string();
  for (const BrightnessTarget &target : brightness_targets_) {
    if (target.brightness_path.string() == brightness_text) return false;
  }
  brightness_targets_.push_back(BrightnessTarget{brightness_path, max_brightness_path, max_brightness});
  if (brightness_path_.empty()) brightness_path_ = brightness_path;
  if (brightness_max_path_.empty()) brightness_max_path_ = max_brightness_path;
  LogSystemControl("discover brightness target: source=" + source +
                   " brightness_path=" + brightness_path.string() +
                   " max_brightness_path=" + max_brightness_path.string() +
                   " max_brightness=" + std::to_string(max_brightness) +
                   " index=" + std::to_string(brightness_targets_.size() - 1));
  return true;
}

bool SystemControlService::WriteBrightnessTargets(int requested_level,
                                                  int &targets_written,
                                                  int &targets_attempted) {
  targets_written = 0;
  targets_attempted = 0;
  if (brightness_targets_.empty()) {
    LogSystemControl("set brightness failed: no discovered sysfs brightness targets");
    return false;
  }

  const int clamped_level = ClampBrightnessLevel(requested_level);
  for (const BrightnessTarget &target : brightness_targets_) {
    int max_brightness = target.max_brightness;
    if (!ReadSmallIntFile(target.max_brightness_path, max_brightness) || max_brightness <= 0) {
      LogSystemControl("set brightness target skipped: could not read max_brightness path=" +
                       target.max_brightness_path.string() +
                       " cached_max_brightness=" + std::to_string(target.max_brightness));
      continue;
    }
    const int raw_value = LevelToSysfsRawBrightness(clamped_level, max_brightness);
    ++targets_attempted;
    LogSystemControl("set brightness target request: level=" + std::to_string(clamped_level) +
                     " raw_value=" + std::to_string(raw_value) +
                     " brightness_path=" + target.brightness_path.string() +
                     " max_brightness=" + std::to_string(max_brightness));
    if (!WriteSmallIntFile(target.brightness_path, raw_value)) {
      LogSystemControl("set brightness target failed: write path=" + target.brightness_path.string() +
                       " raw_value=" + std::to_string(raw_value));
      continue;
    }
    ++targets_written;
    LogSystemControl("set brightness target success: write path=" + target.brightness_path.string() +
                     " raw_value=" + std::to_string(raw_value));
  }

  return targets_written > 0;
}

#include "screen_profile.h"

#include <algorithm>
#include <cctype>
#include <cstdlib>

namespace {
std::string LowerAscii(std::string value) {
  std::transform(value.begin(), value.end(), value.begin(), [](unsigned char ch) {
    return static_cast<char>(std::tolower(ch));
  });
  return value;
}

bool ApplyKnownProfile(const std::string &token, int &width, int &height, std::string &name) {
  const std::string profile = LowerAscii(token);
  if (profile == "1600x1440" || profile == "gkd350h-ultra" || profile == "gkd350h") {
    width = 1600;
    height = 1440;
    name = "1600x1440";
    return true;
  }
  if (profile == "1024x768" || profile == "trimui-brick") {
    width = 1024;
    height = 768;
    name = "1024x768";
    return true;
  }
  if (profile == "720x720" || profile == "rgds" || profile == "rgcubexx") {
    width = 720;
    height = 720;
    name = "720x720";
    return true;
  }
  if (profile == "640x480") {
    width = 640;
    height = 480;
    name = "640x480";
    return true;
  }
  if (profile == "720x480" || profile == "desktop") {
    width = 720;
    height = 480;
    name = "720x480";
    return true;
  }
  return false;
}
}  // namespace

std::string DetectDeviceModelToken() {
  if (const char *value = std::getenv("ROCGALGAME_DEVICE_MODEL"); value && *value) {
    return LowerAscii(value);
  }
#ifdef _WIN32
  return "desktop";
#else
  return {};
#endif
}

ScreenProfile ResolveScreenProfile(const AppConfig &config) {
  ScreenProfile result;
  result.detected_w = config.screen_w;
  result.detected_h = config.screen_h;
  result.screen_w = config.screen_w;
  result.screen_h = config.screen_h;
  result.profile_name = config.screen_profile;

  std::string requested = config.screen_profile;
  if (const char *value = std::getenv("ROCGALGAME_SCREEN_PROFILE"); value && *value) {
    requested = value;
    result.detection_source = "env-profile";
  }
  if (!ApplyKnownProfile(requested, result.screen_w, result.screen_h, result.profile_name)) {
    if (!ApplyKnownProfile(std::to_string(config.screen_w) + "x" + std::to_string(config.screen_h),
                           result.screen_w, result.screen_h, result.profile_name)) {
      result.screen_w = config.screen_w;
      result.screen_h = config.screen_h;
      result.profile_name = requested;
    }
  }
  return result;
}

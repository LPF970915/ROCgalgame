#pragma once

#include "config.h"

#include <string>

struct ScreenProfile {
  int detected_w = 0;
  int detected_h = 0;
  int screen_w = 1600;
  int screen_h = 1440;
  std::string profile_name = "1600x1440";
  std::string detection_source = "config";
};

ScreenProfile ResolveScreenProfile(const AppConfig &config);
std::string DetectDeviceModelToken();

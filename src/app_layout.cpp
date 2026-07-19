#include "app_layout.h"

#include <cmath>

namespace {
constexpr float kFocusScale = 1.045f;

constexpr LayoutMetrics kLayout720x480{};

constexpr LayoutMetrics kLayout640x480{
    640, 480, 16,
    0, 30, 30, 50, 80, 350, 430, 50,
    130, 195, 167, 232, 25, 30, 23, 100,
    36, 2, 4, 24, 160, 0, 35, 28, 20,
    14, 46, 587, 46, 72, 124, 42, 32,
    18, 12, 34, 34, 4, 2, 1.0f};

constexpr LayoutMetrics kLayout720x720{
    720, 720, 20,
    0, 36, 36, 58, 104, 556, 660, 60,
    140, 210, 180, 250, 33, 38, 33, 100,
    36, 2, 4, 24, 240, 0, 42, 32, 26,
    21, 54, 667, 54, 90, 135, 50, 32,
    18, 16, 34, 34, 4, 3, 1.0f};

constexpr LayoutMetrics kLayout1024x768{
    1024, 768, 26,
    0, 48, 48, 80, 128, 560, 688, 80,
    208, 312, 267, 371, 40, 48, 37, 160,
    58, 3, 6, 38, 256, 0, 56, 45, 32,
    22, 74, 939, 74, 115, 198, 67, 32,
    29, 19, 54, 54, 4, 2, 1.6f};

constexpr LayoutMetrics kLayout1600x1440{
    1600, 1440, 40,
    0, 80, 80, 112, 192, 1168, 1360, 80,
    314, 471, 394, 551, 72, 70, 74, 222,
    72, 4, 8, 48, 480, 0, 84, 64, 52,
    48, 104, 1480, 104, 196, 300, 96, 80,
    36, 32, 68, 68, 4, 3, 2.0f};
}  // namespace

const LayoutMetrics &SelectLayoutProfile(int screen_w, int screen_h) {
  if (screen_w == 1600 && screen_h == 1440) return kLayout1600x1440;
  if (screen_w == 1024 && screen_h == 768) return kLayout1024x768;
  if (screen_w == 720 && screen_h == 720) return kLayout720x720;
  if (screen_w == 640 && screen_h == 480) return kLayout640x480;
  return kLayout720x480;
}

LayoutMetrics ResolveLayout(const AppConfig &, const ScreenProfile &profile) {
  return SelectLayoutProfile(profile.screen_w, profile.screen_h);
}

int FocusedCoverWidth(const LayoutMetrics &layout) {
  return static_cast<int>(std::round(static_cast<float>(layout.cover_w) * kFocusScale));
}

int FocusedCoverHeight(const LayoutMetrics &layout) {
  return static_cast<int>(std::round(static_cast<float>(layout.cover_h) * kFocusScale));
}

int GridStepX(const LayoutMetrics &layout) { return layout.cover_w + layout.grid_gap_x; }
int GridStepY(const LayoutMetrics &layout) { return layout.cover_h + layout.grid_gap_y; }

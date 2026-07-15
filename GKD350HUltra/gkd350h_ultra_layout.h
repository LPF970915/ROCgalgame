#pragma once

#include "../src/app_layout.h"

namespace gkd350h_ultra {

inline const LayoutMetrics &Layout1600x1440() {
  // Native GKD350H Ultra shelf geometry, matched to ui/1600x1440.
  static constexpr LayoutMetrics layout{
      1600, 1440, 40,
      0, 80,
      80, 112,
      192, 1168,
      1360, 80,
      314, 471,
      394, 551,
      72, 70,
      74, 222,
      72, 4, 8, 48,
      480, 0, 84,
      64, 52,
      48, 104,
      1480, 104,
      196, 300, 96, 80,
      36, 32, 68, 68,
      4, 3,
      2.0f,
  };
  return layout;
}

}  // namespace gkd350h_ultra

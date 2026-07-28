#include "core_input_protocol.h"

#include <algorithm>
#include <cmath>
#include <iomanip>
#include <sstream>

bool CoreAxisStateNeedsSend(float x, float y, float previous_x,
                            float previous_y, bool has_previous,
                            std::uint64_t elapsed_ms) {
  if (!has_previous || elapsed_ms >= kCoreAxisHeartbeatMs) return true;
  return std::abs(x - previous_x) >= kCoreAxisChangeThreshold ||
         std::abs(y - previous_y) >= kCoreAxisChangeThreshold;
}

std::string EncodeCoreAxisState(float x, float y, std::uint64_t sequence) {
  std::ostringstream output;
  output << "A " << std::fixed << std::setprecision(6)
         << std::clamp(x, -1.0f, 1.0f) << ' '
         << std::clamp(y, -1.0f, 1.0f) << ' ' << sequence << '\n';
  return output.str();
}

CorePointerDelta IntegrateCorePointer(float axis_x, float axis_y,
                                      float elapsed_seconds, float speed,
                                      float acceleration) {
  if (!std::isfinite(axis_x) || !std::isfinite(axis_y) ||
      !std::isfinite(elapsed_seconds) || elapsed_seconds <= 0.0f ||
      !std::isfinite(speed) || speed <= 0.0f ||
      !std::isfinite(acceleration) || acceleration <= 0.0f)
    return {};
  axis_x = std::clamp(axis_x, -1.0f, 1.0f);
  axis_y = std::clamp(axis_y, -1.0f, 1.0f);
  const float magnitude = std::sqrt(axis_x * axis_x + axis_y * axis_y);
  if (magnitude <= 0.0f) return {};
  const float normalized = std::min(magnitude, 1.0f);
  const float distance = speed * elapsed_seconds *
                         std::pow(normalized, acceleration);
  return {axis_x * distance / magnitude, axis_y * distance / magnitude};
}

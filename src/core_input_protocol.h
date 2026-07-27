#pragma once

#include <cstdint>
#include <string>

constexpr float kCoreAxisChangeThreshold = 1.0f / 512.0f;
constexpr std::uint64_t kCoreAxisHeartbeatMs = 150;

struct CorePointerDelta {
  float x = 0.0f;
  float y = 0.0f;
};

bool CoreAxisStateNeedsSend(float x, float y, float previous_x,
                            float previous_y, bool has_previous,
                            std::uint64_t elapsed_ms);
std::string EncodeCoreAxisState(float x, float y, std::uint64_t sequence);
CorePointerDelta IntegrateCorePointer(float axis_x, float axis_y,
                                      float elapsed_seconds, float speed,
                                      float acceleration);

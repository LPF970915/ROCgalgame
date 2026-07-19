#pragma once

#include "app_context.h"
#include "scene_manager.h"

#include <cstdint>

struct AppFrameTiming {
  uint32_t frame_begin_ticks = 0;
  uint32_t now = 0;
  float dt = 0.0f;
};

class AppShell {
public:
  void Initialize(AppContext &context);
  AppContext *Context();
  SceneManager &Scenes();
  bool IsRunning() const;
  void RequestQuit();
  AppFrameTiming BeginFrame(uint32_t &previous_ticks) const;
  void ResetFrameClock(uint32_t &previous_ticks) const;
  void BeginDraw() const;
  void Present() const;

private:
  AppContext *context_ = nullptr;
  SceneManager scenes_;
  bool running_ = true;
};

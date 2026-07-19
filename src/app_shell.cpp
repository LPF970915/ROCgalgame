#include "app_shell.h"

#include <SDL.h>

#include <algorithm>

void AppShell::Initialize(AppContext &context) {
  context_ = &context;
  running_ = true;
}

AppContext *AppShell::Context() { return context_; }
SceneManager &AppShell::Scenes() { return scenes_; }
bool AppShell::IsRunning() const { return running_; }
void AppShell::RequestQuit() { running_ = false; }

AppFrameTiming AppShell::BeginFrame(uint32_t &previous_ticks) const {
  AppFrameTiming frame;
  frame.frame_begin_ticks = SDL_GetTicks();
  frame.now = frame.frame_begin_ticks;
  frame.dt = std::min(0.05f, static_cast<float>(frame.now - previous_ticks) / 1000.0f);
  previous_ticks = frame.now;
  return frame;
}

void AppShell::ResetFrameClock(uint32_t &previous_ticks) const { previous_ticks = SDL_GetTicks(); }

void AppShell::BeginDraw() const {
  if (!context_ || !context_->renderer) return;
  SDL_SetRenderDrawColor(context_->renderer, 0, 0, 0, 255);
  SDL_RenderClear(context_->renderer);
}

void AppShell::Present() const {
  if (context_ && context_->renderer) SDL_RenderPresent(context_->renderer);
}

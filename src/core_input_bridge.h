#pragma once

#include "game_core_adapter.h"
#include "input_manager.h"

#include <memory>
#include <string>

class CoreInputBridge {
public:
  CoreInputBridge(InputManager &input, const EffectiveGameSettings &settings,
                  std::string display = ":2");
  ~CoreInputBridge();

  CoreInputBridge(const CoreInputBridge &) = delete;
  CoreInputBridge &operator=(const CoreInputBridge &) = delete;

  // Returns true when the shared Start+Select exit chord requests shutdown.
  bool Poll();

private:
  struct Impl;
  std::unique_ptr<Impl> impl_;
};

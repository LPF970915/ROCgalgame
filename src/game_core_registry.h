#pragma once

#include "game_core_adapter.h"

#include <array>

class GameCoreRegistry {
public:
  void Register(const IGameCoreAdapter *adapter);
  const IGameCoreAdapter *Find(CoreKind kind) const;

private:
  static size_t Index(CoreKind kind);
  std::array<const IGameCoreAdapter *, 4> adapters_{};
};

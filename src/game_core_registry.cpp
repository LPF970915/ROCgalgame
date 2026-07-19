#include "game_core_registry.h"

size_t GameCoreRegistry::Index(CoreKind kind) {
  const size_t index = static_cast<size_t>(kind);
  return index < 4 ? index : 0;
}

void GameCoreRegistry::Register(const IGameCoreAdapter *adapter) {
  if (!adapter) return;
  adapters_[Index(adapter->Kind())] = adapter;
}

const IGameCoreAdapter *GameCoreRegistry::Find(CoreKind kind) const {
  const IGameCoreAdapter *adapter = adapters_[Index(kind)];
  return adapter && adapter->Kind() == kind ? adapter : nullptr;
}

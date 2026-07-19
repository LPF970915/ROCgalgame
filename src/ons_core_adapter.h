#pragma once

#include "game_core_adapter.h"

class OnsCoreAdapter final : public IGameCoreAdapter {
public:
  CoreKind Kind() const override { return CoreKind::Ons; }
  CoreSpecResult BuildSpec(const AppConfig &config,
                           const GameEntry &game) const override;
};

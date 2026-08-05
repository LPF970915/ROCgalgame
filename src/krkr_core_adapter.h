#pragma once

#include "game_core_adapter.h"

KrkrRuntime ResolveKrkrRuntime(KrkrRuntime configured_runtime);

class KrkrCoreAdapter final : public IGameCoreAdapter {
public:
  CoreKind Kind() const override { return CoreKind::Krkr; }
  CoreSpecResult BuildSpec(const AppConfig &config,
                           const GameEntry &game) const override;
};

#pragma once

#include "config.h"
#include "game_library.h"

bool WriteLaunchRequest(const AppConfig &config, const GameEntry &game);
int LaunchGameAndWait(const AppConfig &config, const GameEntry &game);

#pragma once

#include "cover_cache_runtime.h"
#include "game_library.h"

TextureHandle *ResolveGameCover(SDL_Renderer *renderer, UiAssets &assets,
                                CoverCacheRuntime &cache,
                                const GameEntry &game);

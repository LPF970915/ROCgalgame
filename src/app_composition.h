#pragma once

#include "app_context.h"

AppContext MakeAppContext(SDL_Window *window, SDL_Renderer *renderer,
                          const ScreenProfile &screen_profile, const LayoutMetrics &layout,
                          bool renderer_supports_target_textures);

#include "app_composition.h"

AppContext MakeAppContext(SDL_Window *window, SDL_Renderer *renderer,
                          const ScreenProfile &screen_profile, const LayoutMetrics &layout,
                          bool renderer_supports_target_textures) {
  AppContext context;
  context.window = window;
  context.renderer = renderer;
  context.screen_profile = screen_profile;
  context.layout = &layout;
  context.renderer_supports_target_textures = renderer_supports_target_textures;
  return context;
}

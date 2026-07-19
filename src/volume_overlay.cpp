#include "volume_overlay.h"

#include <algorithm>
#include <string>

void DrawVolumeOverlayRuntime(const VolumeOverlayRenderDeps &deps) {
#ifdef HAVE_SDL2_TTF
  if (!deps.renderer || SDL_TICKS_PASSED(deps.now, deps.display_until)) return;
  const SDL_Color text_color{238, 242, 250, 255};
  TextCacheEntry *text = deps.get_text_texture
                             ? deps.get_text_texture(std::to_string(deps.display_percent), text_color)
                             : nullptr;
  if (!text || !text->texture) return;
  const int x = deps.scale_px ? deps.scale_px(18) : 18;
  const int y = deps.top_bar_y + std::max(0, (deps.top_bar_h - text->h) / 2);
  const SDL_Rect dst{x, y, text->w, text->h};
  SDL_RenderCopy(deps.renderer, text->texture, nullptr, &dst);
#else
  (void)deps;
#endif
}

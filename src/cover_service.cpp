#include "cover_service.h"

TextureHandle *ResolveGameCover(SDL_Renderer *renderer, UiAssets &assets,
                                CoverCacheRuntime &cache,
                                const GameEntry &game) {
  if (TextureHandle *cover = cache.Get(renderer, assets, game.cover_path)) return cover;
  return assets.Get(game.core == CoreKind::Krkr ? "book_cover_pdf.png"
                                                : "book_cover_txt.png");
}

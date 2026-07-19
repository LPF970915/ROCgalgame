#include "shelf_scene.h"

#include <algorithm>
#include <array>
#include <cmath>

namespace {
constexpr std::array<const char *, 4> kNavLabels = {"ONS", "KRKR", "COLLECTIONS", "HISTORY"};

SDL_Color Color(Uint8 r, Uint8 g, Uint8 b, Uint8 a = 255) {
  return SDL_Color{r, g, b, a};
}

void Fill(SDL_Renderer *renderer, const SDL_Rect &rect, SDL_Color color) {
  SDL_SetRenderDrawBlendMode(renderer, SDL_BLENDMODE_BLEND);
  SDL_SetRenderDrawColor(renderer, color.r, color.g, color.b, color.a);
  SDL_RenderFillRect(renderer, &rect);
}

void DrawTextureNative(SDL_Renderer *renderer, TextureHandle *texture, int x, int y) {
  if (!texture || !texture->texture || texture->w <= 0 || texture->h <= 0) return;
  DrawTexture(renderer, texture, SDL_Rect{x, y, texture->w, texture->h});
}

void DrawTextureCrop(SDL_Renderer *renderer, TextureHandle *texture, const SDL_Rect &dst) {
  if (!texture || !texture->texture || texture->w <= 0 || texture->h <= 0 ||
      dst.w <= 0 || dst.h <= 0) return;
  const float src_aspect = static_cast<float>(texture->w) / texture->h;
  const float dst_aspect = static_cast<float>(dst.w) / dst.h;
  SDL_Rect src{0, 0, texture->w, texture->h};
  if (src_aspect > dst_aspect) {
    src.w = std::max(1, static_cast<int>(std::round(texture->h * dst_aspect)));
    src.x = (texture->w - src.w) / 2;
  } else if (src_aspect < dst_aspect) {
    src.h = std::max(1, static_cast<int>(std::round(texture->w / dst_aspect)));
    src.y = (texture->h - src.h) / 2;
  }
  SDL_RenderCopy(renderer, texture->texture, &src, &dst);
}

SDL_Rect OuterFrameRect(const LayoutMetrics &layout, const SDL_Rect &cover_rect) {
  const float sx = static_cast<float>(cover_rect.w) / std::max(1, layout.cover_w);
  const float sy = static_cast<float>(cover_rect.h) / std::max(1, layout.cover_h);
  const int outer_w = std::max(1, static_cast<int>(std::round(layout.card_frame_w * sx)));
  const int outer_h = std::max(1, static_cast<int>(std::round(layout.card_frame_h * sy)));
  const int cx = cover_rect.x + cover_rect.w / 2;
  const int cy = cover_rect.y + cover_rect.h / 2;
  return SDL_Rect{cx - outer_w / 2, cy - outer_h / 2, outer_w, outer_h};
}

void DrawCenteredText(const ShelfSceneRenderServices &services, const std::string &text,
                      const SDL_Rect &rect, SDL_Color color) {
  if (!services.draw_text || !services.measure_text) return;
  int width = 0;
  int height = 0;
  services.measure_text(text, width, height);
  services.draw_text(text, rect.x + std::max(0, (rect.w - width) / 2),
                     rect.y + std::max(0, (rect.h - height) / 2), color);
}

void DrawTitle(const ShelfSceneRenderContext &context, const GameEntry &game,
               const SDL_Rect &cover_rect, bool focused) {
  const auto &services = context.services;
  DrawTexture(context.renderer, services.get_asset ? services.get_asset("book_title_shadow.png") : nullptr,
              cover_rect);
  if (!services.draw_text || !services.measure_text) return;
  const int text_x = cover_rect.x + context.layout.title_text_pad_x;
  const int text_w = std::max(8, cover_rect.w - context.layout.title_text_pad_x * 2);
  const int text_h = std::max(12, context.layout.title_overlay_h - 2);
  const int text_y = cover_rect.y + cover_rect.h - text_h - context.layout.title_text_pad_bottom;
  const SDL_Rect clip{text_x, text_y, text_w, text_h};
  SDL_Rect previous_clip{};
  const SDL_bool had_clip = SDL_RenderIsClipEnabled(context.renderer);
  if (had_clip == SDL_TRUE) SDL_RenderGetClipRect(context.renderer, &previous_clip);
  SDL_RenderSetClipRect(context.renderer, &clip);
  std::string title = game.title;
  int title_w = 0;
  int title_h = 0;
  services.measure_text(title, title_w, title_h);
  if (!focused && title_w > text_w && services.ellipsize) {
    title = services.ellipsize(title, text_w);
    services.measure_text(title, title_w, title_h);
  }
  int draw_x = text_x + std::max(0, (text_w - title_w) / 2);
  if (focused && title_w > text_w) {
    const float cycle = static_cast<float>(title_w + context.layout.title_marquee_gap_px);
    const float offset = cycle > 0.0f ? std::fmod(context.state.title_marquee_offset, cycle) : 0.0f;
    draw_x = text_x - static_cast<int>(offset);
    services.draw_text(title, draw_x + static_cast<int>(cycle),
                       text_y + std::max(0, (text_h - title_h) / 2),
                       Color(248, 250, 255));
  }
  services.draw_text(title, draw_x, text_y + std::max(0, (text_h - title_h) / 2),
                     focused ? Color(248, 250, 255) : Color(230, 236, 248, 244));
  SDL_RenderSetClipRect(context.renderer, had_clip == SDL_TRUE ? &previous_clip : nullptr);
}

void DrawGameCard(const ShelfSceneRenderContext &context, const GameEntry &game,
                  SDL_Rect cover_rect, bool focused) {
  const auto &services = context.services;
  const SDL_Rect outer = OuterFrameRect(context.layout, cover_rect);
  DrawTexture(context.renderer, services.get_asset ? services.get_asset("book_under_shadow.png") : nullptr,
              outer);
  Fill(context.renderer, cover_rect,
       game.core == CoreKind::Krkr ? Color(50, 58, 78) : Color(38, 48, 66));
  TextureHandle *cover = services.get_cover ? services.get_cover(game) : nullptr;
  DrawTextureCrop(context.renderer, cover, cover_rect);
  DrawTitle(context, game, cover_rect, focused);
  if (focused) {
    DrawTexture(context.renderer, services.get_asset ? services.get_asset("book_select.png") : nullptr,
                outer);
  }
}

void DrawPage(const ShelfSceneRenderContext &context, int page, int x_offset,
              bool draw_focus) {
  const auto &state = context.state;
  const auto &games = context.library.Games();
  const int cols = std::max(1, context.layout.grid_cols);
  const int begin = page * cols;
  const int end = std::min(begin + cols * std::max(1, context.layout.visible_rows),
                           static_cast<int>(state.visible_games.size()));
  int focused_index = -1;
  for (int index = begin; index < end; ++index) {
    if (draw_focus && index == state.focus_index) {
      focused_index = index;
      continue;
    }
    const int local = index - begin;
    const int col = local % cols;
    const int row = local / cols;
    const int game_index = state.visible_games[static_cast<size_t>(index)];
    if (game_index < 0 || game_index >= static_cast<int>(games.size())) continue;
    DrawGameCard(context, games[static_cast<size_t>(game_index)],
                 SDL_Rect{context.layout.grid_start_x + col * GridStepX(context.layout) + x_offset,
                          context.layout.grid_start_y + row * GridStepY(context.layout),
                          context.layout.cover_w, context.layout.cover_h}, false);
  }
  if (focused_index >= begin && focused_index < end) {
    const int local = focused_index - begin;
    const int col = local % cols;
    const int row = local / cols;
    const int game_index = state.visible_games[static_cast<size_t>(focused_index)];
    if (game_index < 0 || game_index >= static_cast<int>(games.size())) return;
    const int width = FocusedCoverWidth(context.layout);
    const int height = FocusedCoverHeight(context.layout);
    DrawGameCard(context, games[static_cast<size_t>(game_index)],
                 SDL_Rect{context.layout.grid_start_x + col * GridStepX(context.layout) +
                              (context.layout.cover_w - width) / 2 + x_offset,
                          context.layout.grid_start_y + row * GridStepY(context.layout) +
                              (context.layout.cover_h - height) / 2,
                          width, height}, true);
  }
}

void DrawNavigation(const ShelfSceneRenderContext &context) {
  const auto &services = context.services;
  DrawTextureNative(context.renderer, services.get_asset ? services.get_asset("top_status_bar.png") : nullptr,
                    0, 0);
  DrawTextureNative(context.renderer, services.get_asset ? services.get_asset("bottom_hint_bar.png") : nullptr,
                    0, context.layout.screen_h - context.layout.bottom_bar_h);
  DrawTextureNative(context.renderer, services.get_asset ? services.get_asset("nav_l1_icon.png") : nullptr,
                    context.layout.nav_l1_x, context.layout.nav_l1_y);
  DrawTextureNative(context.renderer, services.get_asset ? services.get_asset("nav_r1_icon.png") : nullptr,
                    context.layout.nav_r1_x, context.layout.nav_r1_y);
  TextureHandle *pill = services.get_asset ? services.get_asset("nav_selected_pill.png") : nullptr;
  if (pill && pill->texture) {
    const int center = context.layout.nav_start_x +
                       context.state.category_index * context.layout.nav_slot_w +
                       context.layout.nav_slot_w / 2;
    DrawTextureNative(context.renderer, pill, center - pill->w / 2, context.layout.nav_y);
  } else {
    const int x = context.layout.nav_start_x +
                  context.state.category_index * context.layout.nav_slot_w + 8;
    Fill(context.renderer,
         SDL_Rect{x, context.layout.nav_y, context.layout.nav_slot_w - 16,
                  context.layout.nav_pill_h},
         Color(105, 113, 130));
  }
  const int nav_height = pill && pill->h > 0 ? pill->h : context.layout.nav_pill_h;
  for (int i = 0; i < static_cast<int>(kNavLabels.size()); ++i) {
    std::string label = kNavLabels[static_cast<size_t>(i)];
    if (services.ellipsize) label = services.ellipsize(label, context.layout.nav_slot_w - 12);
    DrawCenteredText(services, label,
                     SDL_Rect{context.layout.nav_start_x + i * context.layout.nav_slot_w,
                              context.layout.nav_y, context.layout.nav_slot_w, nav_height},
                     Color(238, 242, 250));
  }
}
}  // namespace

bool ShelfScene::FocusedTitleNeedsMarquee(const ShelfSceneRenderContext &context) const {
  const GameEntry *game = FocusedShelfGame(context.state, context.library);
  if (!game || !context.services.measure_text) return false;
  int width = 0;
  int height = 0;
  context.services.measure_text(game->title, width, height);
  return width > std::max(8, context.layout.cover_w - context.layout.title_text_pad_x * 2);
}

void ShelfScene::Draw(const ShelfSceneRenderContext &context) const {
  if (!context.renderer) return;
  DrawTexture(context.renderer,
              context.services.get_asset ? context.services.get_asset("background_main.png") : nullptr,
              SDL_Rect{0, 0, context.layout.screen_w, context.layout.screen_h});
  if (context.state.page_animation < 1.0f &&
      context.state.previous_page != context.state.page) {
    const float eased = 1.0f - std::pow(1.0f - context.state.page_animation, 3.0f);
    const int current_offset = static_cast<int>((1.0f - eased) *
                                                 context.layout.screen_w *
                                                 context.state.page_direction);
    const int previous_offset = current_offset -
                                context.layout.screen_w * context.state.page_direction;
    DrawPage(context, context.state.previous_page, previous_offset, false);
    DrawPage(context, context.state.page, current_offset, true);
  } else {
    DrawPage(context, context.state.page, 0, true);
  }
  DrawNavigation(context);
}

#pragma once

#include "contributor_avatar_runtime.h"
#include "menu_panel.h"
#include "ui_assets.h"

#include <SDL.h>

#include <vector>

void DrawContributorAvatarPanel(
    SDL_Renderer *renderer, UiAssets &assets, const SDL_Rect &preview,
    const std::vector<ContributorAvatarEntry> &entries,
    const ContributorAvatarState &state, const SettingsRuntimeState &menu_state,
    int language_index, const MenuPanelDrawServices &services);

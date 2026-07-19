#pragma once

#include "settings_runtime.h"

using MenuRuntimeInputDeps = SettingsRuntimeInputDeps;
using MenuRuntimeRenderDeps = SettingsRuntimeRenderDeps;
using MenuRuntimeLayout = SettingsRuntimeLayout;

void HandleMenuRuntimeInput(MenuRuntimeInputDeps &deps);
void DrawMenuRuntime(MenuRuntimeRenderDeps &deps);

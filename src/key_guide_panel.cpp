#include "key_guide_panel.h"

#include <array>
#include <string>
#include <utility>

void DrawKeyGuidePanel(const SDL_Rect &preview, int first_row_y,
                       const MenuPanelDrawServices &services) {
  if (services.draw_text) {
    services.draw_text(u8"GKD350H Ultra 按键说明", preview.x + 64, first_row_y - 104,
                       SDL_Color{240, 246, 255, 255}, MenuPanelTextStyle::Menu);
  }
  services.draw_rect(SDL_Rect{preview.x + 36, first_row_y - 36, preview.w - 72, 2},
                     SDL_Color{66, 95, 124, 255}, true, 1);
  const std::array<std::pair<const char *, const char *>, 8> lines = {{
      {"D-Pad", u8"移动书架焦点；在菜单中选择项目或调整选项"},
      {"A", u8"确认；启动当前游戏；执行所选按钮"},
      {"B", u8"返回上一级；关闭当前面板"},
      {"X", u8"将当前游戏加入收藏"},
      {"Y", u8"将当前游戏移出收藏"},
      {"Menu", u8"打开或关闭设置菜单"},
      {"L1 / R1", u8"切换游戏分类"},
      {"Vol- / Vol+", u8"降低或提高系统音量"},
  }};
  int y = first_row_y + 4;
  for (const auto &line : lines) {
    if (services.draw_text) {
      services.draw_text(std::string(line.first) + ":", preview.x + 64, y,
                         SDL_Color{191, 221, 247, 255}, MenuPanelTextStyle::Menu);
      services.draw_text(line.second, preview.x + 310, y + 6,
                         SDL_Color{230, 236, 248, 255}, MenuPanelTextStyle::Body);
    }
    y += 112;
  }
}

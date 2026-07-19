#include "key_guide_panel.h"

#include <array>
#include <algorithm>
#include <string>
#include <utility>
#include <vector>

namespace {
size_t Utf8CharSize(unsigned char lead) {
  if ((lead & 0x80u) == 0) return 1;
  if ((lead & 0xE0u) == 0xC0u) return 2;
  if ((lead & 0xF0u) == 0xE0u) return 3;
  if ((lead & 0xF8u) == 0xF0u) return 4;
  return 1;
}

std::string TrimAsciiSpaces(std::string text) {
  while (!text.empty() && (text.front() == ' ' || text.front() == '\t')) {
    text.erase(text.begin());
  }
  while (!text.empty() && (text.back() == ' ' || text.back() == '\t')) {
    text.pop_back();
  }
  return text;
}

std::vector<std::string> WrapTextByWidth(const std::string &text, int max_width,
                                         const MenuPanelDrawServices &services) {
  std::vector<std::string> lines;
  if (text.empty() || max_width <= 0 || !services.measure_text) {
    if (!text.empty()) lines.push_back(text);
    return lines;
  }

  auto measure = [&](const std::string &value) {
    int width = 0;
    int height = 0;
    services.measure_text(value, MenuPanelTextStyle::Menu, width, height);
    return width;
  };

  std::string current;
  size_t last_break = std::string::npos;
  for (size_t pos = 0; pos < text.size();) {
    const size_t char_size =
        std::min(Utf8CharSize(static_cast<unsigned char>(text[pos])), text.size() - pos);
    const std::string glyph = text.substr(pos, char_size);
    const std::string candidate = current + glyph;
    if (!current.empty() && measure(candidate) > max_width) {
      if (last_break != std::string::npos) {
        std::string line = TrimAsciiSpaces(current.substr(0, last_break));
        std::string remain = TrimAsciiSpaces(current.substr(last_break));
        if (!line.empty()) lines.push_back(line);
        current = remain + glyph;
      } else {
        lines.push_back(TrimAsciiSpaces(current));
        current = glyph;
      }
      last_break = std::string::npos;
      for (size_t i = 0; i < current.size(); ++i) {
        if (current[i] == ' ' || current[i] == '/' || current[i] == ';') {
          last_break = i + 1;
        }
      }
    } else {
      current = candidate;
      if (glyph == " " || glyph == "/" || glyph == ";") last_break = current.size();
    }
    pos += char_size;
  }

  current = TrimAsciiSpaces(current);
  if (!current.empty()) lines.push_back(current);
  return lines;
}
}  // namespace

void DrawKeyGuidePanel(const SDL_Rect &preview, int first_row_y,
                       const MenuPanelDrawServices &services) {
  DrawMenuPanelHeader(preview, first_row_y, u8"GKD350H Ultra 按键说明", services);
  const std::array<std::pair<const char *, const char *>, 8> lines = {{
      {"D-Pad", u8"移动书架焦点；在菜单中选择项目或调整选项"},
      {"A", u8"确认；启动当前游戏；执行所选按钮"},
      {"B", u8"返回上一级；关闭当前面板"},
      {"X", u8"将当前游戏加入收藏"},
      {"Y", u8"将当前游戏移出收藏"},
      {"L1 / R1", u8"切换游戏分类"},
      {"Menu", u8"打开或关闭设置菜单"},
      {"Vol- / Vol+", u8"降低或提高系统音量"},
  }};
  const int left = preview.x + 64;
  const int max_width = std::max(0, preview.w - 128);
  int y = first_row_y + 12;
  for (const auto &line : lines) {
    const std::string text = std::string(line.first) + ": " + line.second;
    for (const std::string &wrapped : WrapTextByWidth(text, max_width, services)) {
      if (services.draw_text) {
        services.draw_text(wrapped, left, y, SDL_Color{191, 221, 247, 255},
                           MenuPanelTextStyle::Menu);
      }
      int width = 0;
      int height = 52;
      if (services.measure_text) {
        services.measure_text(wrapped, MenuPanelTextStyle::Menu, width, height);
      }
      y += height + 16;
    }
    y += 32;
  }
}

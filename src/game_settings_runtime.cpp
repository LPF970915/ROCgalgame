#include "game_settings_runtime.h"

#include "settings_runtime.h"

#include <algorithm>
#include <array>
#include <cmath>
#include <iomanip>
#include <sstream>

namespace {
template <typename T, size_t N>
T CycleValue(T current, const std::array<T, N> &values, int delta) {
  auto found = std::find(values.begin(), values.end(), current);
  int index = found == values.end() ? 0 : static_cast<int>(found - values.begin());
  index = (index + (delta < 0 ? -1 : 1) + static_cast<int>(N)) % static_cast<int>(N);
  return values[static_cast<size_t>(index)];
}

float CycleFloat(float current, const std::array<float, 4> &values, int delta) {
  int index = 0;
  float best = std::abs(current - values.front());
  for (int i = 1; i < static_cast<int>(values.size()); ++i) {
    const float distance = std::abs(current - values[static_cast<size_t>(i)]);
    if (distance < best) {
      best = distance;
      index = i;
    }
  }
  index = (index + (delta < 0 ? -1 : 1) + static_cast<int>(values.size())) %
          static_cast<int>(values.size());
  return values[static_cast<size_t>(index)];
}

bool ApplyDelta(int row, int delta, GameSettingsState &state,
                const GameSettingsCallbacks &callbacks) {
  switch (row) {
    case 0: {
      constexpr std::array<GameAspectMode, 3> values = {
          GameAspectMode::Stretch, GameAspectMode::FillHeight, GameAspectMode::Contain};
      return callbacks.set_aspect &&
             callbacks.set_aspect(CycleValue(state.aspect, values, delta), state);
    }
    case 1: {
      constexpr std::array<GameFilterMode, 4> values = {
          GameFilterMode::Clean, GameFilterMode::Scanline,
          GameFilterMode::CrtSoft, GameFilterMode::Mask};
      return callbacks.set_filter &&
             callbacks.set_filter(CycleValue(state.filter, values, delta), state);
    }
    case 2:
      return callbacks.set_virtual_mouse &&
             callbacks.set_virtual_mouse(delta < 0, state);
    case 3: {
      constexpr std::array<int, 4> values = {480, 720, 960, 1200};
      return callbacks.set_mouse_speed &&
             callbacks.set_mouse_speed(CycleValue(state.mouse_speed, values, delta), state);
    }
    case 4: {
      constexpr std::array<float, 4> values = {1.0f, 1.3f, 1.6f, 2.0f};
      return callbacks.set_mouse_acceleration &&
             callbacks.set_mouse_acceleration(CycleFloat(state.mouse_acceleration, values, delta), state);
    }
    default:
      return false;
  }
}

const char *LabelForLanguage(int language_index, const char *zh, const char *zh_hant,
                             const char *english) {
  if (language_index == 0) return zh;
  if (language_index == 1) return zh_hant;
  return english;
}
}  // namespace

GameAspectMode ParseGameAspectMode(const std::string &value) {
  if (value == "stretch") return GameAspectMode::Stretch;
  if (value == "fill-height") return GameAspectMode::FillHeight;
  return GameAspectMode::Contain;
}

const char *GameAspectConfigValue(GameAspectMode value) {
  switch (value) {
    case GameAspectMode::Stretch: return "stretch";
    case GameAspectMode::FillHeight: return "fill-height";
    case GameAspectMode::Contain: return "contain";
  }
  return "contain";
}

GameFilterMode ParseGameFilterMode(const std::string &value) {
  if (value == "scanline") return GameFilterMode::Scanline;
  if (value == "crt-soft") return GameFilterMode::CrtSoft;
  if (value == "mask") return GameFilterMode::Mask;
  return GameFilterMode::Clean;
}

const char *GameFilterConfigValue(GameFilterMode value) {
  switch (value) {
    case GameFilterMode::Clean: return "clean";
    case GameFilterMode::Scanline: return "scanline";
    case GameFilterMode::CrtSoft: return "crt-soft";
    case GameFilterMode::Mask: return "mask";
  }
  return "clean";
}

GameSettingsState MakeGameSettingsState(const AppConfig &config) {
  return GameSettingsState{ParseGameAspectMode(config.default_aspect),
                           ParseGameFilterMode(config.default_filter),
                           config.virtual_mouse, config.mouse_speed,
                           config.mouse_accel};
}

GameSettingsCallbacks MakeGameSettingsCallbacks(
    ConfigStore &config_store, const std::function<uint32_t()> &now_provider) {
  auto mark_dirty = [&config_store, now_provider]() {
    config_store.MarkDirty(now_provider ? now_provider() : 0);
  };
  return GameSettingsCallbacks{
      [&config_store](GameSettingsState &state) {
        state = MakeGameSettingsState(config_store.Get());
      },
      [&config_store, mark_dirty](GameAspectMode value, GameSettingsState &state) {
        state.aspect = value;
        config_store.Mutable().default_aspect = GameAspectConfigValue(value);
        mark_dirty();
        return true;
      },
      [&config_store, mark_dirty](GameFilterMode value, GameSettingsState &state) {
        state.filter = value;
        config_store.Mutable().default_filter = GameFilterConfigValue(value);
        mark_dirty();
        return true;
      },
      [&config_store, mark_dirty](bool value, GameSettingsState &state) {
        state.virtual_mouse = value;
        config_store.Mutable().virtual_mouse = value;
        mark_dirty();
        return true;
      },
      [&config_store, mark_dirty](int value, GameSettingsState &state) {
        state.mouse_speed = value;
        config_store.Mutable().mouse_speed = value;
        mark_dirty();
        return true;
      },
      [&config_store, mark_dirty](float value, GameSettingsState &state) {
        state.mouse_acceleration = value;
        config_store.Mutable().mouse_accel = value;
        mark_dirty();
        return true;
      },
  };
}

std::string LocalizedGameSettingsLabel(int language_index, int row) {
  switch (row) {
    case 0: return LabelForLanguage(language_index, u8"画面比例", u8"畫面比例", "Aspect Ratio");
    case 1: return LabelForLanguage(language_index, u8"画面滤镜", u8"畫面濾鏡", "Display Filter");
    case 2: return LabelForLanguage(language_index, u8"虚拟鼠标", u8"虛擬滑鼠", "Virtual Mouse");
    case 3: return LabelForLanguage(language_index, u8"鼠标速度", u8"滑鼠速度", "Mouse Speed");
    case 4: return LabelForLanguage(language_index, u8"鼠标加速度", u8"滑鼠加速度", "Mouse Acceleration");
    default: return {};
  }
}

std::string LocalizedGameAspectLabel(int language_index, GameAspectMode value) {
  switch (value) {
    case GameAspectMode::Stretch:
      return LabelForLanguage(language_index, u8"拉伸全屏", u8"拉伸全螢幕", "Stretch");
    case GameAspectMode::FillHeight:
      return LabelForLanguage(language_index, u8"等比适高", u8"等比適高", "Fill Height");
    case GameAspectMode::Contain:
      return LabelForLanguage(language_index, u8"等比完整", u8"等比完整", "Contain");
  }
  return "Contain";
}

std::string LocalizedGameFilterLabel(int language_index, GameFilterMode value) {
  switch (value) {
    case GameFilterMode::Clean:
      return LabelForLanguage(language_index, u8"清晰", u8"清晰", "Clean");
    case GameFilterMode::Scanline:
      return LabelForLanguage(language_index, u8"扫描线", u8"掃描線", "Scanline");
    case GameFilterMode::CrtSoft:
      return "CRT Soft";
    case GameFilterMode::Mask:
      return LabelForLanguage(language_index, u8"像素遮罩", u8"像素遮罩", "Pixel Mask");
  }
  return "Clean";
}

std::string GameMouseAccelerationLabel(float value) {
  std::ostringstream out;
  out << std::fixed << std::setprecision(1) << value << "x";
  return out.str();
}

bool HandleGameSettingsInput(const InputManager &input,
                             SettingsRuntimeState &menu_state,
                             GameSettingsState &state,
                             const GameSettingsCallbacks &callbacks) {
  constexpr int kRows = 5;
  if (input.IsJustPressed(Button::B) || input.IsJustPressed(Button::Menu)) {
    menu_state.panel_active = false;
    menu_state.panel_focus = 0;
    menu_state.panel_button = 0;
    return true;
  }
  if (input.IsJustPressed(Button::Up) || input.IsRepeated(Button::Up)) {
    menu_state.panel_focus = (menu_state.panel_focus + kRows - 1) % kRows;
  }
  if (input.IsJustPressed(Button::Down) || input.IsRepeated(Button::Down)) {
    menu_state.panel_focus = (menu_state.panel_focus + 1) % kRows;
  }
  if (input.IsJustPressed(Button::Left) || input.IsRepeated(Button::Left)) {
    menu_state.panel_button = 0;
    ApplyDelta(menu_state.panel_focus, -1, state, callbacks);
  }
  if (input.IsJustPressed(Button::Right) || input.IsRepeated(Button::Right) ||
      input.IsJustPressed(Button::A)) {
    if (input.IsJustPressed(Button::Right)) menu_state.panel_button = 1;
    const int delta = menu_state.panel_button == 0 && input.IsJustPressed(Button::A) ? -1 : 1;
    ApplyDelta(menu_state.panel_focus, delta, state, callbacks);
  }
  return true;
}

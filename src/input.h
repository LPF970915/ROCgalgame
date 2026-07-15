#pragma once

#include <SDL.h>

#include <array>

constexpr int kButtonCount = 11;

enum class Button {
  Up = 0,
  Down,
  Left,
  Right,
  A,
  B,
  X,
  Y,
  Menu,
  L1,
  R1,
};

struct ButtonState {
  bool down = false;
  bool pressed = false;
  bool released = false;
  float held = 0.0f;
};

class InputManager {
public:
  void BeginFrame(float dt);
  void Reset();
  void HandleEvent(const SDL_Event &event);
  bool Down(Button b) const;
  bool Pressed(Button b) const;
  bool Released(Button b) const;
  bool Repeated(Button b) const;

private:
  void Set(Button b, bool down);
  static Button Invalid();
  static bool IsValid(Button b);
  static Button KeyToButton(SDL_Keycode key);
  static Button ControllerButtonToButton(Uint8 button);
  static Button AxisToButton(Uint8 axis, Sint16 value);

  std::array<ButtonState, kButtonCount> states_{};
  float dt_ = 1.0f / 60.0f;
};

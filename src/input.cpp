#include "input.h"

namespace {
constexpr Sint16 kAxisDeadzone = 12000;
int Index(Button b) { return static_cast<int>(b); }
}

void InputManager::BeginFrame(float dt) {
  dt_ = dt;
  for (auto &state : states_) {
    state.pressed = false;
    state.released = false;
    if (state.down) state.held += dt_;
  }
}

void InputManager::Reset() {
  states_.fill(ButtonState{});
}

void InputManager::HandleEvent(const SDL_Event &event) {
  switch (event.type) {
    case SDL_KEYDOWN:
    case SDL_KEYUP:
      if (!event.key.repeat) Set(KeyToButton(event.key.keysym.sym), event.type == SDL_KEYDOWN);
      break;
    case SDL_CONTROLLERBUTTONDOWN:
    case SDL_CONTROLLERBUTTONUP:
      Set(ControllerButtonToButton(event.cbutton.button), event.type == SDL_CONTROLLERBUTTONDOWN);
      break;
    case SDL_JOYBUTTONDOWN:
    case SDL_JOYBUTTONUP:
      if (event.jbutton.button == 0) Set(Button::B, event.type == SDL_JOYBUTTONDOWN);
      if (event.jbutton.button == 1) Set(Button::A, event.type == SDL_JOYBUTTONDOWN);
      if (event.jbutton.button == 2) Set(Button::X, event.type == SDL_JOYBUTTONDOWN);
      if (event.jbutton.button == 3) Set(Button::Y, event.type == SDL_JOYBUTTONDOWN);
      if (event.jbutton.button == 4) Set(Button::L1, event.type == SDL_JOYBUTTONDOWN);
      if (event.jbutton.button == 5) Set(Button::R1, event.type == SDL_JOYBUTTONDOWN);
      if (event.jbutton.button == 6 || event.jbutton.button == 7) Set(Button::Menu, event.type == SDL_JOYBUTTONDOWN);
      break;
    case SDL_CONTROLLERAXISMOTION:
      if (event.caxis.axis == SDL_CONTROLLER_AXIS_LEFTX) {
        Set(Button::Left, event.caxis.value < -kAxisDeadzone);
        Set(Button::Right, event.caxis.value > kAxisDeadzone);
      } else if (event.caxis.axis == SDL_CONTROLLER_AXIS_LEFTY) {
        Set(Button::Up, event.caxis.value < -kAxisDeadzone);
        Set(Button::Down, event.caxis.value > kAxisDeadzone);
      }
      break;
    case SDL_JOYAXISMOTION:
      if (event.jaxis.axis == 0) {
        Set(Button::Left, event.jaxis.value < -kAxisDeadzone);
        Set(Button::Right, event.jaxis.value > kAxisDeadzone);
      } else if (event.jaxis.axis == 1) {
        Set(Button::Up, event.jaxis.value < -kAxisDeadzone);
        Set(Button::Down, event.jaxis.value > kAxisDeadzone);
      }
      break;
  }
}

bool InputManager::Down(Button b) const { return IsValid(b) && states_[Index(b)].down; }
bool InputManager::Pressed(Button b) const { return IsValid(b) && states_[Index(b)].pressed; }
bool InputManager::Released(Button b) const { return IsValid(b) && states_[Index(b)].released; }
bool InputManager::Repeated(Button b) const {
  if (!IsValid(b)) return false;
  const ButtonState &state = states_[Index(b)];
  if (!state.down || state.held < 0.42f) return false;
  const int current = static_cast<int>((state.held - 0.42f) / 0.09f);
  const int previous = static_cast<int>((state.held - dt_ - 0.42f) / 0.09f);
  return current != previous;
}

void InputManager::Set(Button b, bool down) {
  if (!IsValid(b)) return;
  auto &state = states_[Index(b)];
  if (state.down == down) return;
  state.down = down;
  state.pressed = down;
  state.released = !down;
  if (down) state.held = 0.0f;
}

Button InputManager::Invalid() { return static_cast<Button>(-1); }
bool InputManager::IsValid(Button b) { return Index(b) >= 0 && Index(b) < kButtonCount; }

Button InputManager::KeyToButton(SDL_Keycode key) {
  switch (key) {
    case SDLK_UP: return Button::Up;
    case SDLK_DOWN: return Button::Down;
    case SDLK_LEFT: return Button::Left;
    case SDLK_RIGHT: return Button::Right;
    case SDLK_RETURN:
    case SDLK_SPACE:
    case SDLK_a:
    case SDLK_z: return Button::A;
    case SDLK_ESCAPE:
    case SDLK_b:
    case SDLK_x: return Button::B;
    case SDLK_s: return Button::Y;
    case SDLK_m:
    case SDLK_TAB:
    case SDLK_BACKSPACE: return Button::Menu;
    case SDLK_q: return Button::L1;
    case SDLK_e: return Button::R1;
    default: return Invalid();
  }
}

Button InputManager::ControllerButtonToButton(Uint8 button) {
  switch (button) {
    case SDL_CONTROLLER_BUTTON_DPAD_UP: return Button::Up;
    case SDL_CONTROLLER_BUTTON_DPAD_DOWN: return Button::Down;
    case SDL_CONTROLLER_BUTTON_DPAD_LEFT: return Button::Left;
    case SDL_CONTROLLER_BUTTON_DPAD_RIGHT: return Button::Right;
    case SDL_CONTROLLER_BUTTON_A: return Button::B;
    case SDL_CONTROLLER_BUTTON_B: return Button::A;
    case SDL_CONTROLLER_BUTTON_X: return Button::X;
    case SDL_CONTROLLER_BUTTON_Y: return Button::Y;
    case SDL_CONTROLLER_BUTTON_LEFTSHOULDER: return Button::L1;
    case SDL_CONTROLLER_BUTTON_RIGHTSHOULDER: return Button::R1;
    case SDL_CONTROLLER_BUTTON_START:
    case SDL_CONTROLLER_BUTTON_BACK:
    case SDL_CONTROLLER_BUTTON_GUIDE: return Button::Menu;
    default: return Invalid();
  }
}

Button InputManager::AxisToButton(Uint8 axis, Sint16 value) { return Invalid(); }

#pragma once

#include <SDL.h>

#include <array>
#include <deque>
#include <filesystem>
#include <string>
#include <unordered_map>
#include <vector>

constexpr int kButtonCount = 13;

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
  VolUp,
  VolDown,
};

enum class RawInputSource {
  Keyboard,
  ControllerButton,
  JoystickButton,
  LinuxKey,
};

struct RawInputBinding {
  RawInputSource source = RawInputSource::Keyboard;
  int code = 0;
  std::string device_name;
};

struct ButtonState {
  bool down = false;
  bool pressed = false;
  bool released = false;
  float held = 0.0f;
};

class InputManager {
public:
  InputManager() = default;
  ~InputManager();

  void Initialize(const std::filesystem::path &mapping_path);
  void BeginFrame(float dt);
  void Reset();
  void HandleEvent(const SDL_Event &event);
  bool Down(Button b) const;
  bool Pressed(Button b) const;
  bool Released(Button b) const;
  bool Repeated(Button b) const;
  void ClearRawSamples();
  bool TakeRawSample(RawInputBinding &binding);
  bool ReloadMappings();

  static const char *ButtonName(Button b);
  static std::string SerializeBinding(const RawInputBinding &binding);

private:
  void Set(Button b, bool down);
  static Button Invalid();
  static bool IsValid(Button b);
  static Button KeyToButton(SDL_Keycode key);
  static Button ControllerButtonToButton(Uint8 button);
  static Button AxisToButton(Uint8 axis, Sint16 value);
  Button MappedKey(int code, Button fallback) const;
  Button MappedControllerButton(int code, Button fallback) const;
  Button MappedJoystickButton(int code, Button fallback) const;
  void RecordRawSample(const RawInputBinding &binding);
  void OpenLinuxInputDevices();
  void CloseLinuxInputDevices();
  void PollLinuxInputDevices();

  std::array<ButtonState, kButtonCount> states_{};
  std::deque<RawInputBinding> raw_samples_;
  std::unordered_map<int, Button> key_map_;
  std::unordered_map<int, Button> controller_map_;
  std::unordered_map<int, Button> joystick_map_;
  std::unordered_map<int, Button> linux_key_map_;
  std::filesystem::path mapping_path_;
  std::vector<int> linux_input_fds_;
  std::unordered_map<int, std::string> linux_input_names_;
  float dt_ = 1.0f / 60.0f;
};

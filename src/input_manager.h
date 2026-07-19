#pragma once

#include <SDL.h>

#include <array>
#include <deque>
#include <string>
#include <unordered_map>
#include <vector>

constexpr int kButtonCount = 19;

enum class Button {
  Up,
  Down,
  Left,
  Right,
  A,
  B,
  X,
  Y,
  Menu,
  L1,
  L2,
  R1,
  R2,
  Start,
  Select,
  VolUp,
  VolDown,
  Power,
  Quit,
};

enum class InputProfile {
  DesktopDefault,
  H700Default,
  H70034xxSp,
  H70035xxH,
  TrimuiBrick,
  GKD350HUltra,
  RGDS,
};

enum class RawInputSource {
  Keyboard,
  GameControllerButton,
  JoystickButton,
  GameControllerAxis,
  JoystickAxis,
  JoystickHat,
  LinuxKey,
  LinuxAbs,
};

struct RawInputBinding {
  RawInputSource source = RawInputSource::Keyboard;
  int code = 0;
  int direction = 0;
  std::string device_name;
  bool pressed = true;
};

struct InputCalibrationMapping {
  Button button = static_cast<Button>(-1);
  RawInputBinding binding;
};

struct AxisButtonMap {
  Button negative = static_cast<Button>(-1);
  Button positive = static_cast<Button>(-1);
};

struct HatButtonMap {
  Button up = static_cast<Button>(-1);
  Button down = static_cast<Button>(-1);
  Button left = static_cast<Button>(-1);
  Button right = static_cast<Button>(-1);
};

const char *ButtonName(Button b);
const char *InputProfileName(InputProfile profile);
InputProfile ResolveInputProfile(const std::string &configured_profile,
                                 const std::string &device_model_token,
                                 const std::string &screen_profile_name);
const char *SdlEventName(Uint32 type);
const char *RawInputSourceName(RawInputSource source);
std::string DescribeRawInputBinding(const RawInputBinding &binding);
std::string RawInputBindingKey(const RawInputBinding &binding);
bool RawInputBindingWritable(const RawInputBinding &binding);
bool SaveInputCalibrationMappings(const std::string &mapping_path,
                                  const std::string &device_model_token,
                                  const std::vector<InputCalibrationMapping> &mappings);

struct BtnState {
  bool down = false;
  bool just_pressed = false;
  bool just_released = false;
  bool repeated = false;
  bool long_pressed = false;
  float hold_time = 0.0f;
  float repeat_timer = 0.0f;
  bool repeat_active = false;
};

class InputManager {
public:
  InputManager();
  InputManager(const std::string &mapping_path, InputProfile input_profile);
  ~InputManager();

  void Initialize(const std::string &mapping_path, InputProfile input_profile);
  bool ReloadMappings();

  void BeginFrame(float dt);
  void HandleEvent(const SDL_Event &e);
  bool EndFrame();

  // Compatibility surface for the existing ROCgalgame scene code. New owners
  // should use the explicit state names above.
  bool Down(Button b) const { return IsPressed(b); }
  bool Pressed(Button b) const { return IsJustPressed(b); }
  bool Released(Button b) const { return IsJustReleased(b); }
  bool Repeated(Button b) const { return IsRepeated(b); }
  void Reset() { ResetAll(); }
  void ClearRawSamples() const { ClearCalibrationSamples(); }
  bool TakeRawSample(RawInputBinding &out) const { return TakeCalibrationSample(out); }
  static const char *ButtonName(Button b) { return ::ButtonName(b); }

  bool IsPressed(Button b) const;
  bool IsJustPressed(Button b) const;
  bool IsJustReleased(Button b) const;
  bool IsRepeated(Button b) const;
  bool IsLongPressed(Button b) const;
  float HoldTime(Button b) const;
  bool AnyPressed() const;
  void ResetAll();
  void RefreshDevices();
  void SuppressPowerUntilRelease();
  void ClearCalibrationSamples() const;
  bool TakeCalibrationSample(RawInputBinding &out) const;
  std::string DescribeJoyMap() const;
  std::string DescribePadMap() const;

private:
  static Button InvalidButton();
public:
  static bool IsValid(Button b);
private:
  static Button KeyToButton(SDL_Keycode k);
  Button KeyboardToButton(SDL_Keycode k) const;
  Button PadToButton(uint8_t b) const;
  Button JoyButtonToButton(uint8_t b) const;
  void RecordCalibrationSample(const RawInputBinding &binding) const;
  bool ApplyAxisMap(const std::array<AxisButtonMap, 16> &map, int axis, int value);
  bool HasCalibratedButton(Button b) const;
  void PollDeviceInputEvents();
  void LoadDefaultPadMap(InputProfile input_profile);
  void LoadDefaultJoyMap(InputProfile input_profile);
  static std::string Trim(std::string s);
  static bool ParseButtonName(const std::string &raw, Button &out);
  void LoadOverrides(const std::string &mapping_path);
  void SetDown(Button b, bool down);
  bool ApplyCustomHatMap(uint8_t hat, uint8_t value);
  bool ApplyCustomLinuxAbsMap(int code, int value);
  bool MarkProbeLogged(std::array<bool, 512> &seen, int index);
  bool MarkProbeLogged(std::array<bool, 16> &seen, int index);
  bool ShouldLogProbeKey(SDL_Scancode scancode);
  bool ShouldLogProbePadButton(uint8_t button);
  bool ShouldLogProbeJoyButton(uint8_t button);
  bool ShouldLogProbeHat(uint8_t hat, uint8_t value);
  bool ShouldLogProbePadAxis(int axis, int value);
  bool ShouldLogProbeJoyAxis(int axis, int value);
  void OpenLinuxInputDevices();
  void CloseLinuxInputDevices();
  void PollLinuxInputDevices();
  const BtnState &Get(Button b) const;
  std::string DescribeMap(const std::array<Button, 32> &map, const char *prefix) const;

  std::array<BtnState, kButtonCount> states_{};
  std::array<Button, 32> pad_map_{};
  std::array<Button, 32> joy_map_{};
  std::unordered_map<int, Button> key_map_;
  std::unordered_map<int, Button> linux_key_map_;
  std::array<AxisButtonMap, 16> pad_axis_map_{};
  std::array<AxisButtonMap, 16> joy_axis_map_{};
  std::array<AxisButtonMap, 64> linux_abs_map_{};
  std::array<HatButtonMap, 16> joy_hat_map_{};
  std::array<bool, kButtonCount> calibrated_buttons_{};
  mutable std::deque<RawInputBinding> calibration_samples_;
  InputProfile input_profile_ = InputProfile::DesktopDefault;
  std::array<int, 16> last_pad_axis_values_{};
  std::array<int, 16> last_joy_axis_values_{};
  std::array<bool, 16> pad_axis_seen_{};
  std::array<bool, 16> joy_axis_seen_{};
  std::array<bool, 512> probe_key_seen_{};
  std::array<bool, 512> probe_pad_button_seen_{};
  std::array<bool, 512> probe_joy_button_seen_{};
  std::array<bool, 512> probe_hat_seen_{};
  std::array<bool, 512> probe_pad_axis_seen_{};
  std::array<bool, 512> probe_joy_axis_seen_{};
  std::array<bool, 512> probe_linux_key_seen_{};
  std::vector<int> linux_input_fds_;
  std::unordered_map<int, std::string> linux_input_names_;
  std::string mapping_path_;
  bool full_input_log_enabled_ = false;
  bool power_suppressed_until_release_ = false;
  Uint32 power_suppressed_until_tick_ = 0;
  float dt_ = 1.0f / 60.0f;
};

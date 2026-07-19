#include "input_manager.h"

#include <algorithm>
#include <cctype>
#include <cstdlib>
#include <fstream>
#include <iostream>
#include <cmath>
#include <sstream>
#include <unordered_map>
#include <unordered_set>

#if !defined(_WIN32)
#include <cerrno>
#include <cstring>
#include <dirent.h>
#include <fcntl.h>
#include <linux/input.h>
#include <sys/ioctl.h>
#include <unistd.h>
#endif

bool IsGKD350HUltraProfile(InputProfile profile) {
  return profile == InputProfile::GKD350HUltra;
}

bool IsGKD350HUltraJoypadName(const std::string &name) {
  std::string lower;
  lower.reserve(name.size());
  for (unsigned char ch : name) {
    lower.push_back(static_cast<char>(std::tolower(ch)));
  }
  return lower.find("gkd_atom_joypad") != std::string::npos ||
         lower.find("gamekiddy") != std::string::npos;
}

#if !defined(_WIN32)
bool IsH700Profile(InputProfile profile) {
  return profile == InputProfile::H700Default ||
         profile == InputProfile::H70034xxSp ||
         profile == InputProfile::H70035xxH;
}

int AxisDirectionFromValue(int fd, int code, int value) {
  input_absinfo info{};
  if (fd >= 0 && ioctl(fd, EVIOCGABS(code), &info) == 0 && info.maximum > info.minimum) {
    const int center = info.minimum + (info.maximum - info.minimum) / 2;
    const int range = info.maximum - info.minimum;
    const int deadzone = std::max(range / 6, info.flat * 2);
    const int delta = value - center;
    if (delta < -deadzone) return -1;
    if (delta > deadzone) return 1;
    return 0;
  }
  constexpr int kFallbackDeadzone = 16000;
  if (value < -kFallbackDeadzone) return -1;
  if (value > kFallbackDeadzone) return 1;
  return 0;
}
#endif

const char *ButtonName(Button b) {
  switch (b) {
  case Button::Up: return "Up";
  case Button::Down: return "Down";
  case Button::Left: return "Left";
  case Button::Right: return "Right";
  case Button::A: return "A";
  case Button::B: return "B";
  case Button::X: return "X";
  case Button::Y: return "Y";
  case Button::Menu: return "Menu";
  case Button::L1: return "L1";
  case Button::L2: return "L2";
  case Button::R1: return "R1";
  case Button::R2: return "R2";
  case Button::Start: return "Start";
  case Button::Select: return "Select";
  case Button::VolUp: return "VolUp";
  case Button::VolDown: return "VolDown";
  case Button::Power: return "Power";
  case Button::Quit: return "Quit";
  default: return "Invalid";
  }
}

const char *ButtonNameOrInvalid(Button b) {
  return InputManager::IsValid(b) ? ButtonName(b) : "Invalid";
}

bool FullInputLogEnabled() {
  const char *value = std::getenv("ROCREADER_FULL_INPUT_LOG");
  return value && *value && std::string(value) != "0";
}

const char *InputProfileName(InputProfile profile) {
  switch (profile) {
  case InputProfile::DesktopDefault: return "desktop-default";
  case InputProfile::H700Default: return "h700-default";
  case InputProfile::H70034xxSp: return "h700-34xxsp";
  case InputProfile::H70035xxH: return "h700-35xxh";
  case InputProfile::TrimuiBrick: return "trimui-brick";
  case InputProfile::GKD350HUltra: return "gkd350h-ultra";
  case InputProfile::RGDS: return "rgds";
  default: return "unknown";
  }
}

InputProfile ResolveInputProfile(const std::string &configured_profile,
                                 const std::string &device_model_token,
                                 const std::string &screen_profile_name) {
#ifdef _WIN32
  (void)configured_profile;
  (void)device_model_token;
  (void)screen_profile_name;
  return InputProfile::DesktopDefault;
#else
  auto normalize = [](std::string value) {
    std::string result;
    result.reserve(value.size());
    for (unsigned char ch : value) {
      if (std::isalnum(ch) != 0) result.push_back(static_cast<char>(std::tolower(ch)));
    }
    return result;
  };
  const std::string configured = normalize(configured_profile);
  if (configured == "desktop" || configured == "desktopdefault") return InputProfile::DesktopDefault;
  if (configured == "h700" || configured == "h700default") return InputProfile::H700Default;
  if (configured == "h70034xxsp" || configured == "rg34xxsp" || configured == "34xxsp") {
    return InputProfile::H70034xxSp;
  }
  if (configured == "h70035xxh" || configured == "rg35xxh" || configured == "35xxh") {
    return InputProfile::H70035xxH;
  }
  if (configured == "trimuibrick" || configured == "brick" || configured == "tg3040") {
    return InputProfile::TrimuiBrick;
  }
  if (configured == "gkd350hultra" || configured == "gkd350h" || configured == "gkdatom") {
    return InputProfile::GKD350HUltra;
  }
  if (configured == "rgds") return InputProfile::RGDS;

  const std::string model = normalize(device_model_token);
  if (model == "desktop" || model == "desktopdefault") return InputProfile::DesktopDefault;
  if (model == "rgds") return InputProfile::RGDS;
  if (model == "gkd350hultra" || model == "gkd350h" || model == "gamekiddygkdatom" ||
      model == "gkdatom" || normalize(screen_profile_name) == "1600x1440") {
    return InputProfile::GKD350HUltra;
  }
  if (model == "trimuibrick" || model == "brick" || model == "tg3040" ||
      normalize(screen_profile_name) == "1024x768") {
    return InputProfile::TrimuiBrick;
  }
  if (model == "rg34xxsp" || model == "34xxsp") return InputProfile::H70034xxSp;
  if (model == "rg35xxh" || model == "35xxh") return InputProfile::H70035xxH;
  if (model == "rg28xx" || model == "28xx" || model == "rg34xx" || model == "34xx" ||
      model == "rg35xx" || model == "35xx" || model == "rg35xxplus" ||
      model == "rg35xxpro" || model == "rg35xxsp" || model == "rg40xx" ||
      model == "rg40xxh" || model == "rg40xxv" || model == "rgcubexx") {
    return InputProfile::H700Default;
  }
#if defined(__arm__) || defined(__aarch64__)
  return InputProfile::H700Default;
#endif
  return InputProfile::DesktopDefault;
#endif
}

const char *SdlEventName(Uint32 type) {
  switch (type) {
  case SDL_KEYDOWN: return "SDL_KEYDOWN";
  case SDL_KEYUP: return "SDL_KEYUP";
  case SDL_CONTROLLERBUTTONDOWN: return "SDL_CONTROLLERBUTTONDOWN";
  case SDL_CONTROLLERBUTTONUP: return "SDL_CONTROLLERBUTTONUP";
  case SDL_CONTROLLERAXISMOTION: return "SDL_CONTROLLERAXISMOTION";
  case SDL_JOYBUTTONDOWN: return "SDL_JOYBUTTONDOWN";
  case SDL_JOYBUTTONUP: return "SDL_JOYBUTTONUP";
  case SDL_JOYHATMOTION: return "SDL_JOYHATMOTION";
  case SDL_JOYAXISMOTION: return "SDL_JOYAXISMOTION";
  case SDL_CONTROLLERDEVICEADDED: return "SDL_CONTROLLERDEVICEADDED";
  case SDL_CONTROLLERDEVICEREMOVED: return "SDL_CONTROLLERDEVICEREMOVED";
  case SDL_JOYDEVICEADDED: return "SDL_JOYDEVICEADDED";
  case SDL_JOYDEVICEREMOVED: return "SDL_JOYDEVICEREMOVED";
  default: return "SDL_EVENT_UNKNOWN";
  }
}

const char *RawInputSourceName(RawInputSource source) {
  switch (source) {
  case RawInputSource::Keyboard: return "key";
  case RawInputSource::GameControllerButton: return "pad";
  case RawInputSource::JoystickButton: return "joy";
  case RawInputSource::GameControllerAxis: return "pad_axis";
  case RawInputSource::JoystickAxis: return "joy_axis";
  case RawInputSource::JoystickHat: return "joy_hat";
  case RawInputSource::LinuxKey: return "linux_key";
  case RawInputSource::LinuxAbs: return "linux_abs";
  default: return "unknown";
  }
}

std::string DirectionName(int direction) {
  if (direction < 0) return "neg";
  if (direction > 0) return "pos";
  return "center";
}

std::string HatDirectionName(int direction) {
  switch (direction) {
  case SDL_HAT_UP: return "up";
  case SDL_HAT_DOWN: return "down";
  case SDL_HAT_LEFT: return "left";
  case SDL_HAT_RIGHT: return "right";
  default: return std::to_string(direction);
  }
}

std::string DescribeRawInputBinding(const RawInputBinding &binding) {
  std::ostringstream out;
  out << RawInputSourceName(binding.source) << "." << binding.code;
  if (binding.source == RawInputSource::GameControllerAxis ||
      binding.source == RawInputSource::JoystickAxis ||
      binding.source == RawInputSource::LinuxAbs) {
    out << "." << DirectionName(binding.direction);
  } else if (binding.source == RawInputSource::JoystickHat) {
    out << "." << HatDirectionName(binding.direction);
  }
  if (!binding.device_name.empty()) out << " (" << binding.device_name << ")";
  return out.str();
}

std::string RawInputBindingKey(const RawInputBinding &binding) {
  std::ostringstream out;
  switch (binding.source) {
  case RawInputSource::Keyboard:
    out << "key." << binding.code;
    break;
  case RawInputSource::GameControllerButton:
    out << "pad." << binding.code;
    break;
  case RawInputSource::JoystickButton:
    out << "joy." << binding.code;
    break;
  case RawInputSource::GameControllerAxis:
    out << "pad_axis." << binding.code << "." << DirectionName(binding.direction);
    break;
  case RawInputSource::JoystickAxis:
    out << "joy_axis." << binding.code << "." << DirectionName(binding.direction);
    break;
  case RawInputSource::JoystickHat:
    out << "joy_hat." << binding.code << "." << HatDirectionName(binding.direction);
    break;
  case RawInputSource::LinuxKey:
    out << "linux_key." << binding.code;
    break;
  case RawInputSource::LinuxAbs:
    out << "linux_abs." << binding.code << "." << DirectionName(binding.direction);
    break;
  default:
    break;
  }
  return out.str();
}

bool RawInputBindingWritable(const RawInputBinding &binding) {
  if (binding.code < 0) return false;
  if (binding.source == RawInputSource::GameControllerAxis ||
      binding.source == RawInputSource::JoystickAxis ||
      binding.source == RawInputSource::LinuxAbs) return true;
  if (binding.source == RawInputSource::JoystickHat) {
    return binding.direction == SDL_HAT_UP ||
           binding.direction == SDL_HAT_DOWN ||
           binding.direction == SDL_HAT_LEFT ||
           binding.direction == SDL_HAT_RIGHT;
  }
  return true;
}

bool IsCalibrationMappingOverrideKey(const std::string &key) {
  return key.rfind("joy.", 0) == 0 ||
         key.rfind("pad.", 0) == 0 ||
         key.rfind("key.", 0) == 0 ||
         key.rfind("linux_key.", 0) == 0 ||
         key.rfind("joy_axis.", 0) == 0 ||
         key.rfind("pad_axis.", 0) == 0 ||
         key.rfind("linux_abs.", 0) == 0 ||
         key.rfind("joy_hat.", 0) == 0;
}

bool IsSupportedCalibrationOverrideKey(const std::string &key) {
  return key.rfind("joy.", 0) == 0 ||
         key.rfind("pad.", 0) == 0 ||
         key.rfind("key.", 0) == 0 ||
         key.rfind("joy_axis.", 0) == 0 ||
         key.rfind("pad_axis.", 0) == 0 ||
         key.rfind("linux_key.", 0) == 0 ||
         key.rfind("joy_hat.", 0) == 0 ||
         key.rfind("linux_abs.", 0) == 0;
}

bool IsCalibratedLogicalButton(Button button) {
  switch (button) {
  case Button::Up:
  case Button::Down:
  case Button::Left:
  case Button::Right:
  case Button::A:
  case Button::B:
  case Button::X:
  case Button::Y:
  case Button::Menu:
  case Button::L1:
  case Button::L2:
  case Button::R1:
  case Button::R2:
  case Button::Start:
  case Button::Select:
    return true;
  default:
    return false;
  }
}

int CalibrationKeyPriority(const std::string &key) {
  if (key.rfind("joy.", 0) == 0 || key.rfind("pad.", 0) == 0) return 50;
  if (key.rfind("joy_hat.", 0) == 0) return 40;
  if (key.rfind("joy_axis.", 0) == 0 || key.rfind("pad_axis.", 0) == 0) return 30;
  if (key.rfind("linux_abs.", 0) == 0) return 25;
  if (key.rfind("linux_key.", 0) == 0) return 20;
  if (key.rfind("key.", 0) == 0) return 10;
  return 0;
}

int CalibrationKeyPriority(const std::string &key, Button, InputProfile) {
  return CalibrationKeyPriority(key);
}

bool SaveInputCalibrationMappings(const std::string &mapping_path,
                                  const std::string &device_model_token,
                                  const std::vector<InputCalibrationMapping> &mappings) {
  std::unordered_set<std::string> seen_keys;
  for (const InputCalibrationMapping &mapping : mappings) {
    const std::string key = RawInputBindingKey(mapping.binding);
    if (!InputManager::IsValid(mapping.button) || !RawInputBindingWritable(mapping.binding) ||
        key.empty() || !seen_keys.insert(key).second) {
      return false;
    }
  }

  std::vector<std::string> preserved;
  std::ifstream in(mapping_path);
  std::string line;
  bool in_legacy_calibration = false;
  while (std::getline(in, line)) {
    if (line == "# ROCgalgame calibration begin") {
      in_legacy_calibration = true;
      continue;
    }
    if (line == "# ROCgalgame calibration end") {
      in_legacy_calibration = false;
      continue;
    }
    if (in_legacy_calibration) continue;
    const size_t equals = line.find('=');
    std::string key = equals == std::string::npos ? std::string{} : line.substr(0, equals);
    while (!key.empty() && std::isspace(static_cast<unsigned char>(key.front())) != 0) key.erase(key.begin());
    while (!key.empty() && std::isspace(static_cast<unsigned char>(key.back())) != 0) key.pop_back();
    const bool generated_header = line.rfind("# ROCreader calibrated keymap", 0) == 0 ||
                                  line.rfind("# calibration_version=", 0) == 0 ||
                                  line.rfind("# calibration_model=", 0) == 0;
    if (!generated_header && !IsCalibrationMappingOverrideKey(key)) preserved.push_back(line);
  }
  while (!preserved.empty() && preserved.back().empty()) preserved.pop_back();

  std::ofstream out(mapping_path, std::ios::trunc);
  if (!out) return false;
  for (const std::string &kept : preserved) out << kept << "\n";
  if (!preserved.empty()) out << "\n";
  out << "# ROCreader calibrated keymap\n";
  out << "# calibration_version=5\n";
  out << "# calibration_model=" << (device_model_token.empty() ? "unknown" : device_model_token) << "\n";
  for (const InputCalibrationMapping &mapping : mappings) {
    out << RawInputBindingKey(mapping.binding) << "=" << ButtonName(mapping.button) << "\n";
  }
  out.close();
  return out.good();
}

InputManager::InputManager() {
  pad_map_.fill(InvalidButton());
  joy_map_.fill(InvalidButton());
}

InputManager::InputManager(const std::string &mapping_path, InputProfile input_profile)
    : InputManager() {
  Initialize(mapping_path, input_profile);
}

void InputManager::Initialize(const std::string &mapping_path, InputProfile input_profile) {
  CloseLinuxInputDevices();
  mapping_path_ = mapping_path;
  input_profile_ = input_profile;
  full_input_log_enabled_ = FullInputLogEnabled();
  ResetAll();
  ClearCalibrationSamples();
  key_map_.clear();
  linux_key_map_.clear();
  pad_map_.fill(InvalidButton());
  joy_map_.fill(InvalidButton());
  pad_axis_map_.fill(AxisButtonMap{});
  joy_axis_map_.fill(AxisButtonMap{});
  linux_abs_map_.fill(AxisButtonMap{});
  joy_hat_map_.fill(HatButtonMap{});
  calibrated_buttons_.fill(false);
  LoadDefaultPadMap(input_profile);
  LoadDefaultJoyMap(input_profile);
  LoadOverrides(mapping_path);
  OpenLinuxInputDevices();
}

bool InputManager::ReloadMappings() {
  if (mapping_path_.empty()) return false;
  Initialize(mapping_path_, input_profile_);
  std::ifstream in(mapping_path_);
  return static_cast<bool>(in);
}

InputManager::~InputManager() {
  CloseLinuxInputDevices();
}

void InputManager::BeginFrame(float dt) {
  dt_ = dt;
  for (auto &s : states_) {
    s.just_pressed = false;
    s.just_released = false;
    s.repeated = false;
    s.long_pressed = false;
  }
}

void InputManager::HandleEvent(const SDL_Event &e) {
  if (e.type == SDL_KEYDOWN && !e.key.repeat) {
    if (input_profile_ == InputProfile::RGDS && e.key.keysym.sym == SDLK_AC_BACK) {
      SetDown(Button::Menu, true);
      return;
    }
    RecordCalibrationSample(RawInputBinding{RawInputSource::Keyboard,
                                            static_cast<int>(e.key.keysym.sym),
                                            0,
                                            {},
                                            true});
    const Button mapped = KeyboardToButton(e.key.keysym.sym);
    if (ShouldLogProbeKey(e.key.keysym.scancode)) {
      std::cout << "[native_h700] input probe: type=" << SdlEventName(e.type)
                << " key=" << static_cast<int>(e.key.keysym.sym)
                << " scancode=" << static_cast<int>(e.key.keysym.scancode)
                << " mapped=" << ButtonNameOrInvalid(mapped) << "\n";
    }
    SetDown(mapped, true);
  } else if (e.type == SDL_KEYUP) {
    if (input_profile_ == InputProfile::RGDS && e.key.keysym.sym == SDLK_AC_BACK) {
      SetDown(Button::Menu, false);
      return;
    }
    const Button mapped = KeyboardToButton(e.key.keysym.sym);
    RecordCalibrationSample(RawInputBinding{RawInputSource::Keyboard,
                                            static_cast<int>(e.key.keysym.sym),
                                            0,
                                            {},
                                            false});
    SetDown(mapped, false);
  } else if (e.type == SDL_CONTROLLERBUTTONDOWN) {
    if (input_profile_ == InputProfile::RGDS || IsGKD350HUltraProfile(input_profile_)) return;
    RecordCalibrationSample(RawInputBinding{RawInputSource::GameControllerButton,
                                            static_cast<int>(e.cbutton.button),
                                            0,
                                            {},
                                            true});
    const Button mapped = PadToButton(e.cbutton.button);
    if (ShouldLogProbePadButton(e.cbutton.button)) {
      std::cout << "[native_h700] input probe: type=" << SdlEventName(e.type)
                << " pad_button=" << static_cast<int>(e.cbutton.button)
                << " mapped=" << ButtonNameOrInvalid(mapped) << "\n";
    }
    SetDown(PadToButton(e.cbutton.button), true);
  } else if (e.type == SDL_CONTROLLERBUTTONUP) {
    if (input_profile_ == InputProfile::RGDS || IsGKD350HUltraProfile(input_profile_)) return;
    RecordCalibrationSample(RawInputBinding{RawInputSource::GameControllerButton,
                                            static_cast<int>(e.cbutton.button),
                                            0,
                                            {},
                                            false});
    SetDown(PadToButton(e.cbutton.button), false);
  } else if (e.type == SDL_CONTROLLERAXISMOTION) {
    if (input_profile_ == InputProfile::RGDS || IsGKD350HUltraProfile(input_profile_)) return;
    constexpr int kDeadzone = 16000;
    const int axis = e.caxis.axis;
    const int val = static_cast<int>(e.caxis.value);
    const bool valid_axis = axis >= 0 && axis < static_cast<int>(last_pad_axis_values_.size());
    const int old_val = valid_axis ? last_pad_axis_values_[static_cast<size_t>(axis)] : 0;
    const bool should_log_axis = ShouldLogProbePadAxis(axis, val);
    if (should_log_axis) {
      std::cout << "[native_h700] input probe: type=" << SdlEventName(e.type)
                << " which=" << e.caxis.which
                << " pad_axis=" << axis
                << " value=" << val
                << " old=" << old_val << "\n";
    }
    if (valid_axis) {
      last_pad_axis_values_[static_cast<size_t>(axis)] = val;
      pad_axis_seen_[static_cast<size_t>(axis)] = true;
    }
    RecordCalibrationSample(RawInputBinding{RawInputSource::GameControllerAxis,
                                            axis,
                                            std::abs(val) > kDeadzone ? (val > 0 ? 1 : -1) : 0,
                                            {},
                                            std::abs(val) > kDeadzone});
    if (ApplyAxisMap(pad_axis_map_, axis, val)) return;
    if (axis == SDL_CONTROLLER_AXIS_LEFTX || axis == SDL_CONTROLLER_AXIS_RIGHTX) {
      if (!HasCalibratedButton(Button::Left)) SetDown(Button::Left, val < -kDeadzone);
      if (!HasCalibratedButton(Button::Right)) SetDown(Button::Right, val > kDeadzone);
    } else if (axis == SDL_CONTROLLER_AXIS_LEFTY || axis == SDL_CONTROLLER_AXIS_RIGHTY) {
      if (!HasCalibratedButton(Button::Up)) SetDown(Button::Up, val < -kDeadzone);
      if (!HasCalibratedButton(Button::Down)) SetDown(Button::Down, val > kDeadzone);
    } else if (axis == SDL_CONTROLLER_AXIS_TRIGGERLEFT && !HasCalibratedButton(Button::L2)) {
      SetDown(Button::L2, val > kDeadzone);
    } else if (axis == SDL_CONTROLLER_AXIS_TRIGGERRIGHT && !HasCalibratedButton(Button::R2)) {
      SetDown(Button::R2, val > kDeadzone);
    }
  } else if (e.type == SDL_JOYBUTTONDOWN) {
    if (input_profile_ == InputProfile::RGDS || IsGKD350HUltraProfile(input_profile_)) return;
    RecordCalibrationSample(RawInputBinding{RawInputSource::JoystickButton,
                                            static_cast<int>(e.jbutton.button),
                                            0,
                                            {},
                                            true});
    const Button mapped = JoyButtonToButton(e.jbutton.button);
    if (ShouldLogProbeJoyButton(e.jbutton.button)) {
      std::cout << "[native_h700] input probe: type=" << SdlEventName(e.type)
                << " joy_button=" << static_cast<int>(e.jbutton.button)
                << " mapped=" << ButtonNameOrInvalid(mapped) << "\n";
    }
    SetDown(JoyButtonToButton(e.jbutton.button), true);
  } else if (e.type == SDL_JOYBUTTONUP) {
    if (input_profile_ == InputProfile::RGDS || IsGKD350HUltraProfile(input_profile_)) return;
    RecordCalibrationSample(RawInputBinding{RawInputSource::JoystickButton,
                                            static_cast<int>(e.jbutton.button),
                                            0,
                                            {},
                                            false});
    SetDown(JoyButtonToButton(e.jbutton.button), false);
  } else if (e.type == SDL_JOYHATMOTION) {
    if (input_profile_ == InputProfile::RGDS || IsGKD350HUltraProfile(input_profile_)) return;
    const uint8_t v = e.jhat.value;
    if (ShouldLogProbeHat(e.jhat.hat, v)) {
      std::cout << "[native_h700] input probe: type=" << SdlEventName(e.type)
                << " hat=" << static_cast<int>(e.jhat.hat)
                << " value=" << static_cast<int>(v) << "\n";
    }
    const uint8_t single =
        (v & SDL_HAT_UP) ? SDL_HAT_UP :
        ((v & SDL_HAT_DOWN) ? SDL_HAT_DOWN :
         ((v & SDL_HAT_LEFT) ? SDL_HAT_LEFT :
          ((v & SDL_HAT_RIGHT) ? SDL_HAT_RIGHT : SDL_HAT_CENTERED)));
    if (single != SDL_HAT_CENTERED) {
      RecordCalibrationSample(RawInputBinding{RawInputSource::JoystickHat,
                                              static_cast<int>(e.jhat.hat),
                                              static_cast<int>(single),
                                              {},
                                              true});
    }
    if (ApplyCustomHatMap(e.jhat.hat, v)) return;
    if (!HasCalibratedButton(Button::Up)) SetDown(Button::Up, (v & SDL_HAT_UP) != 0);
    if (!HasCalibratedButton(Button::Down)) SetDown(Button::Down, (v & SDL_HAT_DOWN) != 0);
    if (!HasCalibratedButton(Button::Left)) SetDown(Button::Left, (v & SDL_HAT_LEFT) != 0);
    if (!HasCalibratedButton(Button::Right)) SetDown(Button::Right, (v & SDL_HAT_RIGHT) != 0);
  } else if (e.type == SDL_JOYAXISMOTION) {
    if (input_profile_ == InputProfile::RGDS || IsGKD350HUltraProfile(input_profile_)) return;
    constexpr int kDeadzone = 16000;
    const int axis = e.jaxis.axis;
    const int val = static_cast<int>(e.jaxis.value);
    const bool valid_axis = axis >= 0 && axis < static_cast<int>(last_joy_axis_values_.size());
    const int old_val = valid_axis ? last_joy_axis_values_[static_cast<size_t>(axis)] : 0;
    const bool should_log_axis = ShouldLogProbeJoyAxis(axis, val);
    if (should_log_axis) {
      std::cout << "[native_h700] input probe: type=" << SdlEventName(e.type)
                << " which=" << e.jaxis.which
                << " joy_axis=" << axis
                << " value=" << val
                << " old=" << old_val << "\n";
    }
    if (valid_axis) {
      last_joy_axis_values_[static_cast<size_t>(axis)] = val;
      joy_axis_seen_[static_cast<size_t>(axis)] = true;
    }
    RecordCalibrationSample(RawInputBinding{RawInputSource::JoystickAxis,
                                            axis,
                                            std::abs(val) > kDeadzone ? (val > 0 ? 1 : -1) : 0,
                                            {},
                                            std::abs(val) > kDeadzone});
    if (ApplyAxisMap(joy_axis_map_, axis, val)) return;
    if (axis == 0 || axis == 6) {
      if (!HasCalibratedButton(Button::Left)) SetDown(Button::Left, val < -kDeadzone);
      if (!HasCalibratedButton(Button::Right)) SetDown(Button::Right, val > kDeadzone);
    } else if (axis == 1 || axis == 7) {
      if (!HasCalibratedButton(Button::Up)) SetDown(Button::Up, val < -kDeadzone);
      if (!HasCalibratedButton(Button::Down)) SetDown(Button::Down, val > kDeadzone);
    } else if (input_profile_ == InputProfile::TrimuiBrick &&
               axis == 2 &&
               !HasCalibratedButton(Button::L2)) {
      SetDown(Button::L2, val < -kDeadzone);
    } else if (input_profile_ == InputProfile::TrimuiBrick &&
               axis == 5 &&
               !HasCalibratedButton(Button::R2)) {
      SetDown(Button::R2, val < -kDeadzone);
    }
  }
}

bool InputManager::EndFrame() {
  const bool had_pressed_before_poll = AnyPressed();
  PollDeviceInputEvents();
  bool had_input = false;
  for (auto &s : states_) {
    if (s.just_pressed || s.just_released) had_input = true;
    if (!s.down) {
      s.hold_time = 0.0f;
      s.repeat_timer = 0.0f;
      s.repeat_active = false;
      continue;
    }

    s.hold_time += dt_;
    if (s.hold_time >= 0.8f) s.long_pressed = true;

    if (!s.repeat_active) {
      s.repeat_active = true;
      s.repeat_timer = 0.4f;
      s.repeated = true;
      had_input = true;
    } else {
      s.repeat_timer -= dt_;
      if (s.repeat_timer <= 0.0f) {
        s.repeated = true;
        s.repeat_timer = 0.1f;
        had_input = true;
      }
    }
  }
  return had_input || (!had_pressed_before_poll && AnyPressed());
}

bool InputManager::IsPressed(Button b) const { return Get(b).down; }
bool InputManager::IsJustPressed(Button b) const { return Get(b).just_pressed; }
bool InputManager::IsJustReleased(Button b) const { return Get(b).just_released; }
bool InputManager::IsRepeated(Button b) const { return Get(b).repeated; }
bool InputManager::IsLongPressed(Button b) const { return Get(b).long_pressed; }
float InputManager::HoldTime(Button b) const { return Get(b).hold_time; }

bool InputManager::AnyPressed() const {
  for (const auto &s : states_) {
    if (s.down) return true;
  }
  return false;
}

void InputManager::ResetAll() {
  for (auto &s : states_) {
    s = BtnState{};
  }
}

void InputManager::RefreshDevices() {
  CloseLinuxInputDevices();
  OpenLinuxInputDevices();
  ResetAll();
}

void InputManager::SuppressPowerUntilRelease() {
  power_suppressed_until_release_ = true;
  power_suppressed_until_tick_ = SDL_GetTicks() + 900;
  states_[static_cast<int>(Button::Power)] = BtnState{};
}

std::string InputManager::DescribeJoyMap() const { return DescribeMap(joy_map_, "joy"); }

std::string InputManager::DescribePadMap() const { return DescribeMap(pad_map_, "pad"); }

Button InputManager::InvalidButton() { return static_cast<Button>(-1); }

bool InputManager::IsValid(Button b) {
  const int i = static_cast<int>(b);
  return i >= 0 && i < kButtonCount;
}

Button InputManager::KeyToButton(SDL_Keycode k) {
  constexpr SDL_Keycode kVolumeUpFallback = static_cast<SDL_Keycode>(1073741952);
  constexpr SDL_Keycode kVolumeDownFallback = static_cast<SDL_Keycode>(1073741953);
  switch (k) {
  case SDLK_UP: return Button::Up;
  case SDLK_DOWN: return Button::Down;
  case SDLK_LEFT: return Button::Left;
  case SDLK_RIGHT: return Button::Right;
  case SDLK_a: return Button::A;
  case SDLK_b: return Button::B;
  case SDLK_x: return Button::X;
  case SDLK_y: return Button::Y;
  case SDLK_m: return Button::Menu;
  case SDLK_ESCAPE: return Button::B;
  case SDLK_RETURN: return Button::A;
  case SDLK_BACKSPACE: return Button::Select;
  case SDLK_TAB: return Button::Start;
  case SDLK_q: return Button::L1;
  case SDLK_w: return Button::R1;
  case SDLK_e: return Button::L2;
  case SDLK_r: return Button::R2;
  case SDLK_1: return Button::L1;
  case SDLK_2: return Button::L2;
  case SDLK_4: return Button::R1;
  case SDLK_3: return Button::R2;
  case SDLK_z: return Button::Start;
  case SDLK_c: return Button::Select;
#ifdef SDLK_VOLUMEUP
  case SDLK_VOLUMEUP: return Button::VolUp;
#endif
#ifdef SDLK_VOLUMEDOWN
  case SDLK_VOLUMEDOWN: return Button::VolDown;
#endif
#ifdef SDLK_POWER
  case SDLK_POWER: return Button::Power;
#endif
  case kVolumeUpFallback: return Button::VolUp;
  case kVolumeDownFallback: return Button::VolDown;
#ifdef SDLK_PLUS
  case SDLK_PLUS: return Button::VolUp;
#endif
#ifdef SDLK_KP_PLUS
  case SDLK_KP_PLUS: return Button::VolUp;
#endif
#ifdef SDLK_MINUS
  case SDLK_MINUS: return Button::VolDown;
#endif
#ifdef SDLK_KP_MINUS
  case SDLK_KP_MINUS: return Button::VolDown;
#endif
  default: return static_cast<Button>(-1);
  }
}

Button InputManager::KeyboardToButton(SDL_Keycode k) const {
  const auto it = key_map_.find(static_cast<int>(k));
  if (it != key_map_.end()) return it->second;
  return KeyToButton(k);
}

Button InputManager::PadToButton(uint8_t b) const {
  if (b >= pad_map_.size()) return InvalidButton();
  return pad_map_[b];
}

Button InputManager::JoyButtonToButton(uint8_t b) const {
  if (b >= joy_map_.size()) return InvalidButton();
  return joy_map_[b];
}

void InputManager::RecordCalibrationSample(const RawInputBinding &binding) const {
  if (!RawInputBindingWritable(binding)) return;
  const std::string key = RawInputBindingKey(binding);
  if (binding.source == RawInputSource::GameControllerAxis ||
      binding.source == RawInputSource::JoystickAxis ||
      binding.source == RawInputSource::LinuxAbs) {
    for (auto &existing : calibration_samples_) {
      if (existing.source == binding.source &&
          existing.code == binding.code &&
          existing.direction == binding.direction) {
        existing = binding;
        return;
      }
    }
  }
  for (const auto &existing : calibration_samples_) {
    if (RawInputBindingKey(existing) == key && existing.pressed == binding.pressed) return;
  }
  calibration_samples_.push_back(binding);
  while (calibration_samples_.size() > 32) calibration_samples_.pop_front();
}

void InputManager::ClearCalibrationSamples() const {
  calibration_samples_.clear();
}

bool InputManager::TakeCalibrationSample(RawInputBinding &out) const {
  while (!calibration_samples_.empty()) {
    RawInputBinding binding = calibration_samples_.front();
    calibration_samples_.pop_front();
    if (!RawInputBindingWritable(binding)) continue;
    out = std::move(binding);
    return true;
  }
  return false;
}

bool InputManager::ApplyAxisMap(const std::array<AxisButtonMap, 16> &map, int axis, int value) {
  if (axis < 0 || axis >= static_cast<int>(map.size())) return false;
  constexpr int kDeadzone = 16000;
  const AxisButtonMap &entry = map[static_cast<size_t>(axis)];
  bool handled = false;
  if (IsValid(entry.negative)) {
    SetDown(entry.negative, value < -kDeadzone);
    handled = true;
  }
  if (IsValid(entry.positive)) {
    SetDown(entry.positive, value > kDeadzone);
    handled = true;
  }
  return handled;
}

bool InputManager::HasCalibratedButton(Button b) const {
  if (!IsValid(b)) return false;
  return calibrated_buttons_[static_cast<size_t>(static_cast<int>(b))];
}

bool InputManager::ApplyCustomHatMap(uint8_t hat, uint8_t value) {
  if (hat >= joy_hat_map_.size()) return false;
  const HatButtonMap &entry = joy_hat_map_[hat];
  bool handled = false;
  if (IsValid(entry.up)) {
    SetDown(entry.up, (value & SDL_HAT_UP) != 0);
    handled = true;
  }
  if (IsValid(entry.down)) {
    SetDown(entry.down, (value & SDL_HAT_DOWN) != 0);
    handled = true;
  }
  if (IsValid(entry.left)) {
    SetDown(entry.left, (value & SDL_HAT_LEFT) != 0);
    handled = true;
  }
  if (IsValid(entry.right)) {
    SetDown(entry.right, (value & SDL_HAT_RIGHT) != 0);
    handled = true;
  }
  return handled;
}

bool InputManager::ApplyCustomLinuxAbsMap(int code, int value) {
  if (code < 0 || code >= static_cast<int>(linux_abs_map_.size())) return false;
  const AxisButtonMap &entry = linux_abs_map_[static_cast<size_t>(code)];
  bool handled = false;
  if (IsValid(entry.negative)) {
    SetDown(entry.negative, value < 0);
    handled = true;
  }
  if (IsValid(entry.positive)) {
    SetDown(entry.positive, value > 0);
    handled = true;
  }
  return handled;
}

void InputManager::PollDeviceInputEvents() {
  PollLinuxInputDevices();
}

void InputManager::LoadDefaultPadMap(InputProfile input_profile) {
  pad_map_[SDL_CONTROLLER_BUTTON_DPAD_UP] = Button::Up;
  pad_map_[SDL_CONTROLLER_BUTTON_DPAD_DOWN] = Button::Down;
  pad_map_[SDL_CONTROLLER_BUTTON_DPAD_LEFT] = Button::Left;
  pad_map_[SDL_CONTROLLER_BUTTON_DPAD_RIGHT] = Button::Right;
  if (input_profile == InputProfile::TrimuiBrick ||
      input_profile == InputProfile::GKD350HUltra) {
    pad_map_[SDL_CONTROLLER_BUTTON_A] = Button::B;
    pad_map_[SDL_CONTROLLER_BUTTON_B] = Button::A;
    pad_map_[SDL_CONTROLLER_BUTTON_X] = Button::Y;
    pad_map_[SDL_CONTROLLER_BUTTON_Y] = Button::X;
  } else {
    pad_map_[SDL_CONTROLLER_BUTTON_A] = Button::A;
    pad_map_[SDL_CONTROLLER_BUTTON_B] = Button::B;
    pad_map_[SDL_CONTROLLER_BUTTON_X] = Button::X;
    pad_map_[SDL_CONTROLLER_BUTTON_Y] = Button::Y;
  }
  pad_map_[SDL_CONTROLLER_BUTTON_LEFTSHOULDER] = Button::L1;
  pad_map_[SDL_CONTROLLER_BUTTON_RIGHTSHOULDER] = Button::R1;
  pad_map_[SDL_CONTROLLER_BUTTON_LEFTSTICK] = Button::L2;
  pad_map_[SDL_CONTROLLER_BUTTON_RIGHTSTICK] = Button::R2;
  pad_map_[SDL_CONTROLLER_BUTTON_BACK] = Button::Select;
  pad_map_[SDL_CONTROLLER_BUTTON_START] = Button::Start;
}

void InputManager::LoadDefaultJoyMap(InputProfile input_profile) {
  joy_map_[0] = Button::A;
  joy_map_[1] = Button::B;
  joy_map_[4] = Button::L1;
  joy_map_[5] = Button::R1;
  if (input_profile == InputProfile::GKD350HUltra) {
    joy_map_[0] = Button::B;
    joy_map_[1] = Button::A;
    joy_map_[2] = Button::X;
    joy_map_[3] = Button::Y;
    joy_map_[6] = Button::L2;
    joy_map_[7] = Button::R2;
    joy_map_[8] = Button::Menu;
    joy_map_[9] = Button::Start;
    joy_map_[10] = Button::Select;
    joy_map_[12] = Button::Up;
    joy_map_[13] = Button::Down;
    joy_map_[14] = Button::Left;
    joy_map_[15] = Button::Right;
  } else if (input_profile == InputProfile::H70034xxSp) {
    joy_map_[2] = Button::Y;
    joy_map_[3] = Button::X;
    joy_map_[6] = Button::Select;
    joy_map_[7] = Button::Start;
    joy_map_[8] = Button::Menu;
    joy_map_[9] = Button::Menu;
    joy_map_[10] = Button::L2;
    joy_map_[11] = Button::R2;
    joy_map_[12] = Button::Select;
    joy_map_[13] = Button::Start;
    joy_map_[14] = Button::Power;
    joy_map_[15] = Button::VolDown;
    joy_map_[16] = Button::VolUp;
  } else if (input_profile == InputProfile::TrimuiBrick) {
    joy_map_[0] = Button::B;
    joy_map_[1] = Button::A;
    joy_map_[2] = Button::Y;
    joy_map_[3] = Button::X;
    joy_map_[6] = Button::Select;
    joy_map_[7] = Button::Start;
    joy_map_[8] = Button::Menu;
    joy_map_[9] = Button::L2;
    joy_map_[10] = Button::R2;
    joy_map_[11] = InvalidButton();
    joy_map_[12] = Button::Select;
    joy_map_[13] = InvalidButton();
    joy_map_[14] = Button::Power;
    joy_map_[15] = Button::VolDown;
    joy_map_[16] = Button::VolUp;
  } else if (input_profile == InputProfile::H70035xxH) {
    joy_map_[2] = Button::Y;
    joy_map_[3] = Button::X;
    joy_map_[6] = Button::Select;
    joy_map_[7] = Button::Start;
    joy_map_[8] = Button::Menu;
    joy_map_[9] = Button::Menu;
    joy_map_[10] = Button::L2;
    joy_map_[11] = Button::R2;
    joy_map_[12] = Button::Select;
    joy_map_[13] = Button::Start;
    joy_map_[14] = Button::Power;
    joy_map_[15] = Button::VolDown;
    joy_map_[16] = Button::VolUp;
  } else if (input_profile == InputProfile::H700Default) {
    joy_map_[2] = Button::Y;
    joy_map_[3] = Button::X;
    joy_map_[6] = Button::Select;
    joy_map_[7] = Button::Start;
    joy_map_[8] = Button::Menu;
    joy_map_[9] = Button::L2;
    joy_map_[10] = Button::R2;
    joy_map_[11] = Button::Menu;
    joy_map_[12] = Button::Select;
    joy_map_[13] = Button::Start;
    joy_map_[14] = Button::Power;
    joy_map_[15] = Button::VolDown;
    joy_map_[16] = Button::VolUp;
  } else {
    joy_map_[2] = Button::Y;
    joy_map_[3] = Button::X;
    joy_map_[6] = Button::Select;
    joy_map_[7] = Button::Start;
    joy_map_[8] = Button::Menu;
    joy_map_[9] = Button::L2;
    joy_map_[10] = Button::R2;
    joy_map_[11] = Button::Menu;
    joy_map_[12] = Button::Select;
    joy_map_[13] = Button::Start;
    joy_map_[15] = Button::VolDown;
    joy_map_[16] = Button::VolUp;
  }
}

std::string InputManager::Trim(std::string s) {
  size_t b = 0;
  while (b < s.size() && std::isspace(static_cast<unsigned char>(s[b]))) ++b;
  size_t e = s.size();
  while (e > b && std::isspace(static_cast<unsigned char>(s[e - 1]))) --e;
  return s.substr(b, e - b);
}

bool InputManager::ParseButtonName(const std::string &raw, Button &out) {
  std::string n;
  n.reserve(raw.size());
  for (char c : raw) {
    if (c == ' ' || c == '\t' || c == '-') continue;
    n.push_back(static_cast<char>(std::toupper(static_cast<unsigned char>(c))));
  }
  if (n == "UP") out = Button::Up;
  else if (n == "DOWN") out = Button::Down;
  else if (n == "LEFT") out = Button::Left;
  else if (n == "RIGHT") out = Button::Right;
  else if (n == "A") out = Button::A;
  else if (n == "B") out = Button::B;
  else if (n == "X") out = Button::X;
  else if (n == "Y") out = Button::Y;
  else if (n == "MENU") out = Button::Menu;
  else if (n == "L1") out = Button::L1;
  else if (n == "L2") out = Button::L2;
  else if (n == "R1") out = Button::R1;
  else if (n == "R2") out = Button::R2;
  else if (n == "START") out = Button::Start;
  else if (n == "SELECT") out = Button::Select;
  else if (n == "VOLUP" || n == "VOLUMEUP") out = Button::VolUp;
  else if (n == "VOLDOWN" || n == "VOLUMEDOWN") out = Button::VolDown;
  else if (n == "POWER") out = Button::Power;
  else if (n == "QUIT" || n == "RG") out = Button::Quit;
  else if (n == "NONE" || n == "DISABLED" || n == "INVALID") out = InvalidButton();
  else return false;
  return true;
}

void InputManager::LoadOverrides(const std::string &mapping_path) {
  std::ifstream in(mapping_path);
  if (!in) return;
  std::string line;
  std::vector<std::string> lines;
  bool has_calibration_header = false;
  bool supported_calibration_version = false;
  while (std::getline(in, line)) {
    if (line.rfind("# ROCreader calibrated keymap", 0) == 0) has_calibration_header = true;
    if (line.rfind("# calibration_version=", 0) == 0) {
      const int calibration_version = std::atoi(line.substr(22).c_str());
      supported_calibration_version =
          calibration_version == 2 ||
          calibration_version == 3 ||
          calibration_version == 4 ||
          calibration_version == 5;
    }
    lines.push_back(line);
  }

  std::unordered_set<std::string> selected_calibration_keys;
  if (has_calibration_header && supported_calibration_version) {
    struct SelectedCalibrationKey {
      std::string key;
      int priority = -1;
    };
    std::unordered_map<int, SelectedCalibrationKey> selected_by_button;
    for (std::string candidate_line : lines) {
      candidate_line = Trim(candidate_line);
      if (candidate_line.empty() || candidate_line[0] == '#' || candidate_line[0] == ';') continue;
      const size_t eq = candidate_line.find('=');
      if (eq == std::string::npos) continue;
      const std::string key = Trim(candidate_line.substr(0, eq));
      if (!IsCalibrationMappingOverrideKey(key) || !IsSupportedCalibrationOverrideKey(key)) continue;
      Button mapped = InvalidButton();
      if (!ParseButtonName(Trim(candidate_line.substr(eq + 1)), mapped) ||
          !IsCalibratedLogicalButton(mapped)) {
        continue;
      }
      const bool trimui_calibrated_shoulder =
          input_profile_ == InputProfile::TrimuiBrick &&
          (mapped == Button::L2 || mapped == Button::R2);
      if (trimui_calibrated_shoulder) {
        continue;
      }
      const int priority = CalibrationKeyPriority(key, mapped, input_profile_);
      SelectedCalibrationKey &selected = selected_by_button[static_cast<int>(mapped)];
      if (priority > selected.priority) {
        selected.key = key;
        selected.priority = priority;
      }
    }
    calibrated_buttons_.fill(false);
    for (const auto &item : selected_by_button) {
      const SelectedCalibrationKey &selected = item.second;
      if (selected.key.empty()) continue;
      selected_calibration_keys.insert(selected.key);
    }
    auto should_clear_button = [&](Button button) {
      return IsCalibratedLogicalButton(button) &&
             selected_by_button.find(static_cast<int>(button)) != selected_by_button.end();
    };
    for (Button &mapped : pad_map_) {
      if (should_clear_button(mapped)) mapped = InvalidButton();
    }
    for (Button &mapped : joy_map_) {
      if (should_clear_button(mapped)) mapped = InvalidButton();
    }
    auto clear_axis_map = [&](auto &map) {
      for (AxisButtonMap &entry : map) {
        if (should_clear_button(entry.negative)) entry.negative = InputManager::InvalidButton();
        if (should_clear_button(entry.positive)) entry.positive = InputManager::InvalidButton();
      }
    };
    clear_axis_map(pad_axis_map_);
    clear_axis_map(joy_axis_map_);
    clear_axis_map(linux_abs_map_);
    for (HatButtonMap &entry : joy_hat_map_) {
      if (should_clear_button(entry.up)) entry.up = InvalidButton();
      if (should_clear_button(entry.down)) entry.down = InvalidButton();
      if (should_clear_button(entry.left)) entry.left = InvalidButton();
      if (should_clear_button(entry.right)) entry.right = InvalidButton();
    }
  }

  for (std::string line : lines) {
    line = Trim(line);
    if (line.empty() || line[0] == '#' || line[0] == ';') continue;
    const size_t eq = line.find('=');
    if (eq == std::string::npos) continue;
    const std::string key = Trim(line.substr(0, eq));
    const std::string val = Trim(line.substr(eq + 1));

    // ROCgalgame originally stored overrides as Button=Source:Code[:Direction].
    // Keep those files readable while writing the canonical ROCreader format.
    Button legacy_button = InvalidButton();
    if (ParseButtonName(key, legacy_button) && IsValid(legacy_button)) {
      std::vector<std::string> parts;
      std::stringstream values(val);
      std::string part;
      while (std::getline(values, part, ':')) parts.push_back(Trim(part));
      if (parts.size() >= 2) {
        for (std::string &value : parts) {
          std::transform(value.begin(), value.end(), value.begin(),
                         [](unsigned char ch) { return static_cast<char>(std::tolower(ch)); });
        }
        int code = -1;
        try { code = std::stoi(parts[1]); } catch (...) { code = -1; }
        int direction = 0;
        if (parts.size() >= 3) {
          const std::string &raw_direction = parts[2];
          if (raw_direction == "neg" || raw_direction == "negative" || raw_direction == "left" ||
              raw_direction == "up" || raw_direction == "-") direction = -1;
          else if (raw_direction == "pos" || raw_direction == "positive" || raw_direction == "right" ||
                   raw_direction == "down" || raw_direction == "+") direction = 1;
          else {
            try { direction = std::stoi(raw_direction); } catch (...) { direction = 0; }
          }
        }
        if (code >= 0) {
          if (parts[0] == "key") key_map_[code] = legacy_button;
          else if (parts[0] == "pad" && code < static_cast<int>(pad_map_.size())) pad_map_[code] = legacy_button;
          else if (parts[0] == "joy" && code < static_cast<int>(joy_map_.size())) joy_map_[code] = legacy_button;
          else if (parts[0] == "linux_key") linux_key_map_[code] = legacy_button;
          else if ((parts[0] == "pad_axis" || parts[0] == "joy_axis" || parts[0] == "linux_abs") &&
                   direction != 0) {
            AxisButtonMap *axis_map = nullptr;
            if (parts[0] == "pad_axis" && code < static_cast<int>(pad_axis_map_.size())) {
              axis_map = &pad_axis_map_[static_cast<size_t>(code)];
            } else if (parts[0] == "joy_axis" && code < static_cast<int>(joy_axis_map_.size())) {
              axis_map = &joy_axis_map_[static_cast<size_t>(code)];
            } else if (parts[0] == "linux_abs" && code < static_cast<int>(linux_abs_map_.size())) {
              axis_map = &linux_abs_map_[static_cast<size_t>(code)];
            }
            if (axis_map) {
              if (direction < 0) axis_map->negative = legacy_button;
              else axis_map->positive = legacy_button;
            }
          } else if (parts[0] == "joy_hat" && code < static_cast<int>(joy_hat_map_.size())) {
            HatButtonMap &hat = joy_hat_map_[static_cast<size_t>(code)];
            if (parts.size() >= 3 && parts[2] == "up") hat.up = legacy_button;
            else if (parts.size() >= 3 && parts[2] == "down") hat.down = legacy_button;
            else if (parts.size() >= 3 && parts[2] == "left") hat.left = legacy_button;
            else if (parts.size() >= 3 && parts[2] == "right") hat.right = legacy_button;
          }
        }
      }
      continue;
    }
    if (has_calibration_header && !supported_calibration_version &&
        IsCalibrationMappingOverrideKey(key)) {
      continue;
    }
    if (has_calibration_header && supported_calibration_version &&
        IsCalibrationMappingOverrideKey(key) &&
        !IsSupportedCalibrationOverrideKey(key)) {
      continue;
    }
    if (has_calibration_header && supported_calibration_version &&
        IsCalibrationMappingOverrideKey(key) &&
        selected_calibration_keys.find(key) == selected_calibration_keys.end()) {
      continue;
    }
    Button mapped = InvalidButton();
    if (!ParseButtonName(val, mapped)) continue;
    bool applied = false;
    if (key.rfind("joy.", 0) == 0) {
      const int idx = std::atoi(key.substr(4).c_str());
      if (idx >= 0 && idx < static_cast<int>(joy_map_.size())) {
        joy_map_[idx] = mapped;
        applied = true;
      }
    } else if (key.rfind("pad.", 0) == 0) {
      const int idx = std::atoi(key.substr(4).c_str());
      if (idx >= 0 && idx < static_cast<int>(pad_map_.size())) {
        pad_map_[idx] = mapped;
        applied = true;
      }
    } else if (key.rfind("key.", 0) == 0) {
      const int idx = std::atoi(key.substr(4).c_str());
      key_map_[idx] = mapped;
      applied = true;
    } else if (key.rfind("linux_key.", 0) == 0) {
      const int idx = std::atoi(key.substr(10).c_str());
      linux_key_map_[idx] = mapped;
      applied = true;
    } else if (key.rfind("joy_axis.", 0) == 0 || key.rfind("pad_axis.", 0) == 0 ||
               key.rfind("linux_abs.", 0) == 0) {
      const bool is_pad = key.rfind("pad_axis.", 0) == 0;
      const bool is_linux_abs = key.rfind("linux_abs.", 0) == 0;
      const size_t prefix_len = is_pad ? 9u : (is_linux_abs ? 10u : 9u);
      const size_t dot = key.find('.', prefix_len);
      if (dot == std::string::npos) continue;
      const int idx = std::atoi(key.substr(prefix_len, dot - prefix_len).c_str());
      const std::string dir = key.substr(dot + 1);
      AxisButtonMap *entry = nullptr;
      if (is_pad && idx >= 0 && idx < static_cast<int>(pad_axis_map_.size())) {
        entry = &pad_axis_map_[static_cast<size_t>(idx)];
      } else if (!is_pad && !is_linux_abs && idx >= 0 && idx < static_cast<int>(joy_axis_map_.size())) {
        entry = &joy_axis_map_[static_cast<size_t>(idx)];
      } else if (is_linux_abs && idx >= 0 && idx < static_cast<int>(linux_abs_map_.size())) {
        entry = &linux_abs_map_[static_cast<size_t>(idx)];
      }
      if (!entry) continue;
      if (dir == "neg" || dir == "negative" || dir == "-") {
        entry->negative = mapped;
        applied = true;
      } else if (dir == "pos" || dir == "positive" || dir == "+") {
        entry->positive = mapped;
        applied = true;
      }
    } else if (key.rfind("joy_hat.", 0) == 0) {
      const size_t prefix_len = 8;
      const size_t dot = key.find('.', prefix_len);
      if (dot == std::string::npos) continue;
      const int idx = std::atoi(key.substr(prefix_len, dot - prefix_len).c_str());
      if (idx < 0 || idx >= static_cast<int>(joy_hat_map_.size())) continue;
      HatButtonMap &entry = joy_hat_map_[static_cast<size_t>(idx)];
      const std::string dir = key.substr(dot + 1);
      if (dir == "up") {
        entry.up = mapped;
        applied = true;
      } else if (dir == "down") {
        entry.down = mapped;
        applied = true;
      } else if (dir == "left") {
        entry.left = mapped;
        applied = true;
      } else if (dir == "right") {
        entry.right = mapped;
        applied = true;
      }
    }
    if (applied &&
        has_calibration_header &&
        supported_calibration_version &&
        IsCalibrationMappingOverrideKey(key) &&
        IsCalibratedLogicalButton(mapped)) {
      calibrated_buttons_[static_cast<size_t>(static_cast<int>(mapped))] = true;
    }
  }
}

void InputManager::SetDown(Button b, bool down) {
  if (!IsValid(b)) return;
  if (b == Button::Power && power_suppressed_until_release_) {
    const bool suppress_expired = SDL_TICKS_PASSED(SDL_GetTicks(), power_suppressed_until_tick_);
    if (!down || suppress_expired) {
      power_suppressed_until_release_ = false;
      power_suppressed_until_tick_ = 0;
      if (!down) {
        states_[static_cast<int>(Button::Power)] = BtnState{};
        return;
      }
    } else {
      states_[static_cast<int>(Button::Power)] = BtnState{};
      return;
    }
  }
  const int button_index = static_cast<int>(b);
  BtnState &s = states_[button_index];
  if (down && !s.down) {
    s.down = true;
    s.just_pressed = true;
    s.hold_time = 0.0f;
  } else if (!down && s.down) {
    s.down = false;
    s.just_released = true;
  }
}

bool InputManager::MarkProbeLogged(std::array<bool, 512> &seen, int index) {
  if (!full_input_log_enabled_ || index < 0 || index >= static_cast<int>(seen.size())) return false;
  bool &slot = seen[static_cast<size_t>(index)];
  if (slot) return false;
  slot = true;
  return true;
}

bool InputManager::MarkProbeLogged(std::array<bool, 16> &seen, int index) {
  if (!full_input_log_enabled_ || index < 0 || index >= static_cast<int>(seen.size())) return false;
  bool &slot = seen[static_cast<size_t>(index)];
  if (slot) return false;
  slot = true;
  return true;
}

bool InputManager::ShouldLogProbeKey(SDL_Scancode scancode) {
  return MarkProbeLogged(probe_key_seen_, static_cast<int>(scancode));
}

bool InputManager::ShouldLogProbePadButton(uint8_t button) {
  return MarkProbeLogged(probe_pad_button_seen_, static_cast<int>(button));
}

bool InputManager::ShouldLogProbeJoyButton(uint8_t button) {
  return MarkProbeLogged(probe_joy_button_seen_, static_cast<int>(button));
}

bool InputManager::ShouldLogProbeHat(uint8_t hat, uint8_t value) {
  if (!full_input_log_enabled_ || value == SDL_HAT_CENTERED) return false;
  const int index = static_cast<int>(hat) * 16 + static_cast<int>(value);
  return MarkProbeLogged(probe_hat_seen_, index);
}

bool InputManager::ShouldLogProbePadAxis(int axis, int value) {
  constexpr int kDeadzone = 16000;
  if (std::abs(value) <= kDeadzone) return false;
  const int direction = value > 0 ? 1 : 0;
  return MarkProbeLogged(probe_pad_axis_seen_, axis * 2 + direction);
}

bool InputManager::ShouldLogProbeJoyAxis(int axis, int value) {
  constexpr int kDeadzone = 16000;
  if (std::abs(value) <= kDeadzone) return false;
  const int direction = value > 0 ? 1 : 0;
  return MarkProbeLogged(probe_joy_axis_seen_, axis * 2 + direction);
}

void InputManager::OpenLinuxInputDevices() {
#if !defined(_WIN32)
  if (input_profile_ == InputProfile::DesktopDefault) return;
  DIR *dir = opendir("/dev/input");
  if (!dir) return;
  while (dirent *entry = readdir(dir)) {
    if (std::strncmp(entry->d_name, "event", 5) != 0) continue;
    const std::string path = std::string("/dev/input/") + entry->d_name;
    const int fd = open(path.c_str(), O_RDONLY | O_NONBLOCK | O_CLOEXEC);
    if (fd < 0) continue;
    char device_name[256] = {};
    if (ioctl(fd, EVIOCGNAME(sizeof(device_name)), device_name) < 0) {
      device_name[0] = '\0';
    }
    linux_input_fds_.push_back(fd);
    linux_input_names_[fd] = device_name;
    if (full_input_log_enabled_) {
      std::cout << "[native_h700] input probe: opened linux_input=" << path
                << " name=" << device_name << "\n";
    }
  }
  closedir(dir);
#endif
}

void InputManager::CloseLinuxInputDevices() {
#if !defined(_WIN32)
  for (int fd : linux_input_fds_) {
    if (fd >= 0) close(fd);
  }
#endif
  linux_input_fds_.clear();
  linux_input_names_.clear();
}

void InputManager::PollLinuxInputDevices() {
#if !defined(_WIN32)
  for (int fd : linux_input_fds_) {
    const auto name_it = linux_input_names_.find(fd);
    const std::string linux_device_name =
        name_it == linux_input_names_.end() ? std::string{} : name_it->second;
    const bool gkd_profile = IsGKD350HUltraProfile(input_profile_);
    const bool gkd_joypad = gkd_profile && IsGKD350HUltraJoypadName(linux_device_name);
    while (true) {
      input_event event{};
      const ssize_t n = read(fd, &event, sizeof(event));
      if (n == static_cast<ssize_t>(sizeof(event))) {
        if (event.type == EV_ABS && (input_profile_ == InputProfile::RGDS ||
                                     IsH700Profile(input_profile_) ||
                                     gkd_profile)) {
          if (gkd_profile && !gkd_joypad) continue;
          const int code = static_cast<int>(event.code);
          const int value = static_cast<int>(event.value);
          const int dir = AxisDirectionFromValue(fd, code, value);
          RecordCalibrationSample(RawInputBinding{RawInputSource::LinuxAbs,
                                                  code,
                                                  dir,
                                                  linux_device_name,
                                                  dir != 0});
          if (ApplyCustomLinuxAbsMap(code, dir)) continue;
          if (IsGKD350HUltraProfile(input_profile_) && code == ABS_X) {
            SetDown(Button::Left, dir < 0);
            SetDown(Button::Right, dir > 0);
          } else if (IsGKD350HUltraProfile(input_profile_) && code == ABS_Y) {
            SetDown(Button::Up, dir < 0);
            SetDown(Button::Down, dir > 0);
          } else if (code == ABS_HAT0X) {
            SetDown(Button::Left, value < 0);
            SetDown(Button::Right, value > 0);
          } else if (code == ABS_HAT0Y) {
            SetDown(Button::Up, value < 0);
            SetDown(Button::Down, value > 0);
          } else if (input_profile_ != InputProfile::RGDS) {
            continue;
          } else if (code == ABS_Z) {
            SetDown(Button::Left, dir < 0);
            SetDown(Button::Right, dir > 0);
          } else if (code == ABS_RX) {
            SetDown(Button::Up, dir < 0);
            SetDown(Button::Down, dir > 0);
          } else if (code == ABS_RZ) {
            SetDown(Button::Y, dir < 0);
            SetDown(Button::A, dir > 0);
          } else if (code == ABS_RY) {
            SetDown(Button::X, dir < 0);
            SetDown(Button::B, dir > 0);
          }
          continue;
        }

        if (event.type == EV_KEY) {
          const int code = static_cast<int>(event.code);
          // Linux input EV_KEY uses:
          //   0 = release
          //   1 = press
          //   2 = auto-repeat while held
          // Treating 2 as a fresh press causes power-key wake to retrigger
          // screen-off immediately on H700, creating a wake/sleep loop.
          if (event.value == 2) continue;
          const bool down = event.value == 1;
          Button mapped = InvalidButton();
          const auto override_it = linux_key_map_.find(code);
          if (override_it != linux_key_map_.end()) {
            mapped = override_it->second;
          } else if (input_profile_ == InputProfile::RGDS) {
            switch (code) {
              case 304: mapped = Button::A; break;
              case 305: mapped = Button::B; break;
              case 307: mapped = Button::X; break;
              case 306: mapped = Button::Y; break;
              case 308: mapped = Button::L1; break;
              case 309: mapped = Button::R1; break;
              case 314: mapped = Button::L2; break;
              case 315: mapped = Button::R2; break;
              case 310: mapped = Button::Select; break;
              case 311: mapped = Button::Start; break;
              case 312: mapped = Button::Quit; break;
              case KEY_POWER: mapped = Button::Power; break;
              case KEY_BACK: mapped = Button::Menu; break;
              case KEY_VOLUMEUP: mapped = Button::VolUp; break;
              case KEY_VOLUMEDOWN: mapped = Button::VolDown; break;
              default: break;
            }
          } else if (gkd_profile) {
            if (gkd_joypad) {
              switch (code) {
                case BTN_SOUTH: mapped = Button::B; break;
                case BTN_EAST: mapped = Button::A; break;
                case BTN_NORTH: mapped = Button::X; break;
                case BTN_WEST: mapped = Button::Y; break;
                case BTN_TL: mapped = Button::L1; break;
                case BTN_TR: mapped = Button::R1; break;
                case BTN_TL2: mapped = Button::L2; break;
                case BTN_TR2: mapped = Button::R2; break;
                case BTN_SELECT: mapped = Button::Select; break;
                case BTN_START: mapped = Button::Start; break;
                case BTN_MODE: mapped = Button::Menu; break;
                case BTN_DPAD_UP: mapped = Button::Up; break;
                case BTN_DPAD_DOWN: mapped = Button::Down; break;
                case BTN_DPAD_LEFT: mapped = Button::Left; break;
                case BTN_DPAD_RIGHT: mapped = Button::Right; break;
                default: break;
              }
            }
            if (!IsValid(mapped)) {
              switch (code) {
                case KEY_POWER: mapped = Button::Power; break;
                case KEY_BACK: mapped = Button::Menu; break;
                case KEY_VOLUMEUP: mapped = Button::VolUp; break;
                case KEY_VOLUMEDOWN: mapped = Button::VolDown; break;
                default: break;
              }
            }
          } else if (IsH700Profile(input_profile_)) {
            // H700 firmwares also report the same physical buttons through SDL.
            // The evdev BTN_* names are not stable across these devices, so do
            // not map shoulders/face/start/select here or they override the
            // per-profile SDL joy maps with scrambled buttons.
            switch (code) {
              case KEY_UP:
                mapped = Button::Up;
                break;
              case KEY_DOWN:
                mapped = Button::Down;
                break;
              case KEY_LEFT:
                mapped = Button::Left;
                break;
              case KEY_RIGHT:
                mapped = Button::Right;
                break;
              case KEY_BACK:
                mapped = Button::Menu;
                break;
              case KEY_POWER:
                mapped = Button::Power;
                break;
              case KEY_VOLUMEUP:
                mapped = Button::VolUp;
                break;
              case KEY_VOLUMEDOWN:
                mapped = Button::VolDown;
                break;
              default:
                break;
            }
          } else if (code == KEY_POWER) {
            mapped = Button::Power;
          }
          RecordCalibrationSample(RawInputBinding{RawInputSource::LinuxKey,
                                                  code,
                                                  0,
                                                  linux_device_name,
                                                  down});
          if (full_input_log_enabled_ && down && MarkProbeLogged(probe_linux_key_seen_, code)) {
            std::cout << "[native_h700] input probe: type=LINUX_EV_KEY"
                      << " code=" << code
                      << " value=" << event.value
                      << " mapped=" << ButtonNameOrInvalid(mapped) << "\n";
          }
          SetDown(mapped, down);
        }
        continue;
      }
      if (n < 0 && (errno == EAGAIN || errno == EWOULDBLOCK)) break;
      break;
    }
  }
#endif
}


const BtnState &InputManager::Get(Button b) const {
  static BtnState empty;
  if (!IsValid(b)) return empty;
  return states_[static_cast<int>(b)];
}

std::string InputManager::DescribeMap(const std::array<Button, 32> &map, const char *prefix) const {
  std::ostringstream out;
  bool first = true;
  for (size_t i = 0; i < map.size(); ++i) {
    const Button mapped = map[i];
    if (!IsValid(mapped)) continue;
    if (!first) out << ' ';
    out << prefix << '.' << i << '=' << ButtonName(mapped);
    first = false;
  }
  return out.str();
}

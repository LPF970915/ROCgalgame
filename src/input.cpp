#include "input.h"

#include <algorithm>
#include <cctype>
#include <cstring>
#include <fstream>
#include <sstream>

#ifndef _WIN32
#include <dirent.h>
#include <fcntl.h>
#include <linux/input.h>
#include <sys/ioctl.h>
#include <unistd.h>
#endif

namespace {
constexpr Sint16 kAxisDeadzone = 12000;
int Index(Button b) { return static_cast<int>(b); }

std::string TrimAscii(std::string text) {
  while (!text.empty() && std::isspace(static_cast<unsigned char>(text.front()))) text.erase(text.begin());
  while (!text.empty() && std::isspace(static_cast<unsigned char>(text.back()))) text.pop_back();
  return text;
}

std::string LowerAscii(std::string text) {
  std::transform(text.begin(), text.end(), text.begin(),
                 [](unsigned char ch) { return static_cast<char>(std::tolower(ch)); });
  return text;
}

Button ParseButton(const std::string &name) {
  const std::string key = LowerAscii(TrimAscii(name));
  if (key == "up") return Button::Up;
  if (key == "down") return Button::Down;
  if (key == "left") return Button::Left;
  if (key == "right") return Button::Right;
  if (key == "a") return Button::A;
  if (key == "b") return Button::B;
  if (key == "x") return Button::X;
  if (key == "y") return Button::Y;
  if (key == "menu") return Button::Menu;
  if (key == "l1") return Button::L1;
  if (key == "r1") return Button::R1;
  if (key == "volup" || key == "volumeup") return Button::VolUp;
  if (key == "voldown" || key == "volumedown") return Button::VolDown;
  return static_cast<Button>(-1);
}
}

InputManager::~InputManager() { CloseLinuxInputDevices(); }

void InputManager::Initialize(const std::filesystem::path &mapping_path) {
  mapping_path_ = mapping_path;
  ReloadMappings();
  OpenLinuxInputDevices();
}

void InputManager::BeginFrame(float dt) {
  dt_ = dt;
  for (auto &state : states_) {
    state.pressed = false;
    state.released = false;
    if (state.down) state.held += dt_;
  }
  PollLinuxInputDevices();
}

void InputManager::Reset() {
  states_.fill(ButtonState{});
}

void InputManager::HandleEvent(const SDL_Event &event) {
  switch (event.type) {
    case SDL_KEYDOWN:
    case SDL_KEYUP:
      if (!event.key.repeat) {
        const bool down = event.type == SDL_KEYDOWN;
        if (down) RecordRawSample(RawInputBinding{RawInputSource::Keyboard, static_cast<int>(event.key.keysym.sym), {}});
        Set(MappedKey(static_cast<int>(event.key.keysym.sym), KeyToButton(event.key.keysym.sym)), down);
      }
      break;
    case SDL_CONTROLLERBUTTONDOWN:
    case SDL_CONTROLLERBUTTONUP:
      if (event.type == SDL_CONTROLLERBUTTONDOWN) {
        RecordRawSample(RawInputBinding{RawInputSource::ControllerButton, event.cbutton.button, {}});
      }
      Set(MappedControllerButton(event.cbutton.button, ControllerButtonToButton(event.cbutton.button)),
          event.type == SDL_CONTROLLERBUTTONDOWN);
      break;
    case SDL_JOYBUTTONDOWN:
    case SDL_JOYBUTTONUP: {
      const bool down = event.type == SDL_JOYBUTTONDOWN;
      if (down) RecordRawSample(RawInputBinding{RawInputSource::JoystickButton, event.jbutton.button, {}});
      Button fallback = Invalid();
      if (event.jbutton.button == 0) fallback = Button::B;
      if (event.jbutton.button == 1) fallback = Button::A;
      if (event.jbutton.button == 2) fallback = Button::X;
      if (event.jbutton.button == 3) fallback = Button::Y;
      if (event.jbutton.button == 4) fallback = Button::L1;
      if (event.jbutton.button == 5) fallback = Button::R1;
      if (event.jbutton.button == 6 || event.jbutton.button == 7) fallback = Button::Menu;
      Set(MappedJoystickButton(event.jbutton.button, fallback), down);
      break;
    }
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

void InputManager::ClearRawSamples() { raw_samples_.clear(); }

bool InputManager::TakeRawSample(RawInputBinding &binding) {
  if (raw_samples_.empty()) return false;
  binding = raw_samples_.front();
  raw_samples_.pop_front();
  return true;
}

bool InputManager::ReloadMappings() {
  key_map_.clear();
  controller_map_.clear();
  joystick_map_.clear();
  linux_key_map_.clear();
  std::ifstream in(mapping_path_);
  if (!in) return false;
  std::string line;
  while (std::getline(in, line)) {
    line = TrimAscii(line);
    if (line.empty() || line[0] == '#' || line[0] == ';') continue;
    const size_t equals = line.find('=');
    if (equals == std::string::npos) continue;
    const Button button = ParseButton(line.substr(0, equals));
    if (!IsValid(button)) continue;
    const std::string binding = LowerAscii(TrimAscii(line.substr(equals + 1)));
    const size_t colon = binding.find(':');
    if (colon == std::string::npos) continue;
    int code = 0;
    try { code = std::stoi(binding.substr(colon + 1)); } catch (...) { continue; }
    const std::string source = binding.substr(0, colon);
    if (source == "key") key_map_[code] = button;
    else if (source == "pad") controller_map_[code] = button;
    else if (source == "joy") joystick_map_[code] = button;
    else if (source == "linux_key") linux_key_map_[code] = button;
  }
  return true;
}

const char *InputManager::ButtonName(Button b) {
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
    case Button::R1: return "R1";
    case Button::VolUp: return "VolUp";
    case Button::VolDown: return "VolDown";
  }
  return "Invalid";
}

std::string InputManager::SerializeBinding(const RawInputBinding &binding) {
  const char *source = "key";
  if (binding.source == RawInputSource::ControllerButton) source = "pad";
  else if (binding.source == RawInputSource::JoystickButton) source = "joy";
  else if (binding.source == RawInputSource::LinuxKey) source = "linux_key";
  return std::string(source) + ":" + std::to_string(binding.code);
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
  constexpr SDL_Keycode kVolumeUpFallback = static_cast<SDL_Keycode>(1073741952);
  constexpr SDL_Keycode kVolumeDownFallback = static_cast<SDL_Keycode>(1073741953);
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
    case kVolumeUpFallback: return Button::VolUp;
    case kVolumeDownFallback: return Button::VolDown;
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

Button InputManager::MappedKey(int code, Button fallback) const {
  const auto it = key_map_.find(code);
  return it == key_map_.end() ? fallback : it->second;
}

Button InputManager::MappedControllerButton(int code, Button fallback) const {
  const auto it = controller_map_.find(code);
  return it == controller_map_.end() ? fallback : it->second;
}

Button InputManager::MappedJoystickButton(int code, Button fallback) const {
  const auto it = joystick_map_.find(code);
  return it == joystick_map_.end() ? fallback : it->second;
}

void InputManager::RecordRawSample(const RawInputBinding &binding) {
  if (!raw_samples_.empty()) {
    const RawInputBinding &last = raw_samples_.back();
    if (last.source == binding.source && last.code == binding.code && last.device_name == binding.device_name) return;
  }
  raw_samples_.push_back(binding);
  if (raw_samples_.size() > 32) raw_samples_.pop_front();
}

void InputManager::OpenLinuxInputDevices() {
#ifndef _WIN32
  CloseLinuxInputDevices();
  DIR *dir = opendir("/dev/input");
  if (!dir) return;
  while (dirent *entry = readdir(dir)) {
    if (std::strncmp(entry->d_name, "event", 5) != 0) continue;
    const std::string path = std::string("/dev/input/") + entry->d_name;
    const int fd = open(path.c_str(), O_RDONLY | O_NONBLOCK | O_CLOEXEC);
    if (fd < 0) continue;
    char name[256] = {};
    ioctl(fd, EVIOCGNAME(sizeof(name)), name);
    linux_input_fds_.push_back(fd);
    linux_input_names_[fd] = name;
  }
  closedir(dir);
#endif
}

void InputManager::CloseLinuxInputDevices() {
#ifndef _WIN32
  for (int fd : linux_input_fds_) if (fd >= 0) close(fd);
#endif
  linux_input_fds_.clear();
  linux_input_names_.clear();
}

void InputManager::PollLinuxInputDevices() {
#ifndef _WIN32
  for (int fd : linux_input_fds_) {
    while (true) {
      input_event event{};
      const ssize_t count = read(fd, &event, sizeof(event));
      if (count != static_cast<ssize_t>(sizeof(event))) break;
      if (event.type != EV_KEY || event.value == 2) continue;
      const bool down = event.value != 0;
      if (down) {
        RecordRawSample(RawInputBinding{RawInputSource::LinuxKey, event.code, linux_input_names_[fd]});
      }
      Button button = Invalid();
      const auto mapped = linux_key_map_.find(event.code);
      if (mapped != linux_key_map_.end()) button = mapped->second;
      else if (event.code == KEY_VOLUMEUP) button = Button::VolUp;
      else if (event.code == KEY_VOLUMEDOWN) button = Button::VolDown;
      Set(button, down);
    }
  }
#endif
}

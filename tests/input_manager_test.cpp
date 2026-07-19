#include "input_manager.h"

#include <cassert>
#include <filesystem>
#include <fstream>
#include <iostream>
#include <iterator>
#include <string>

namespace {
SDL_Event KeyEvent(Uint32 type, SDL_Keycode key) {
  SDL_Event event{};
  event.type = type;
  event.key.type = type;
  event.key.keysym.sym = key;
  return event;
}

SDL_Event ControllerButtonEvent(Uint32 type, Uint8 button) {
  SDL_Event event{};
  event.type = type;
  event.cbutton.type = type;
  event.cbutton.button = button;
  return event;
}

SDL_Event JoyButtonEvent(Uint32 type, Uint8 button) {
  SDL_Event event{};
  event.type = type;
  event.jbutton.type = type;
  event.jbutton.button = button;
  return event;
}

SDL_Event JoyAxisEvent(Uint8 axis, Sint16 value) {
  SDL_Event event{};
  event.type = SDL_JOYAXISMOTION;
  event.jaxis.type = SDL_JOYAXISMOTION;
  event.jaxis.axis = axis;
  event.jaxis.value = value;
  return event;
}

SDL_Event JoyHatEvent(Uint8 hat, Uint8 value) {
  SDL_Event event{};
  event.type = SDL_JOYHATMOTION;
  event.jhat.type = SDL_JOYHATMOTION;
  event.jhat.hat = hat;
  event.jhat.value = value;
  return event;
}

void WriteFile(const std::filesystem::path &path, const std::string &contents) {
  std::ofstream out(path, std::ios::trunc);
  assert(out);
  out << contents;
  assert(out.good());
}
}  // namespace

int main() {
  assert(ResolveInputProfile("desktop-default", {}, {}) == InputProfile::DesktopDefault);
  assert(ResolveInputProfile("h700-default", {}, {}) == InputProfile::H700Default);
  assert(ResolveInputProfile("h700-34xxsp", {}, {}) == InputProfile::H70034xxSp);
  assert(ResolveInputProfile("h700-35xxh", {}, {}) == InputProfile::H70035xxH);
  assert(ResolveInputProfile("trimui-brick", {}, {}) == InputProfile::TrimuiBrick);
  assert(ResolveInputProfile("gkd350h-ultra", {}, {}) == InputProfile::GKD350HUltra);
  assert(ResolveInputProfile("rgds", {}, {}) == InputProfile::RGDS);
  assert(ResolveInputProfile("auto", "rg34xx-sp", {}) == InputProfile::H70034xxSp);
  assert(ResolveInputProfile("auto", "rg35xx-h", {}) == InputProfile::H70035xxH);
  assert(ResolveInputProfile("auto", {}, "1600x1440") == InputProfile::GKD350HUltra);
  assert(ResolveInputProfile("auto", "desktop", "720x480") == InputProfile::DesktopDefault);
  assert(ResolveInputProfile("auto", "unknown-device", "720x480") == InputProfile::DesktopDefault);

  const std::filesystem::path test_dir = std::filesystem::path("build") / "input_manager_test_data";
  std::filesystem::create_directories(test_dir);
  const std::filesystem::path empty_map = test_dir / "empty.ini";
  WriteFile(empty_map, "# no overrides\n");

  InputManager desktop(empty_map.string(), InputProfile::DesktopDefault);
  desktop.BeginFrame(0.016f);
  desktop.HandleEvent(ControllerButtonEvent(SDL_CONTROLLERBUTTONDOWN, SDL_CONTROLLER_BUTTON_A));
  desktop.EndFrame();
  assert(desktop.IsPressed(Button::A));
  assert(desktop.IsJustPressed(Button::A));
  assert(desktop.IsRepeated(Button::A));

  desktop.BeginFrame(0.4f);
  desktop.EndFrame();
  assert(desktop.IsRepeated(Button::A));
  desktop.BeginFrame(0.4f);
  desktop.EndFrame();
  assert(desktop.IsLongPressed(Button::A));
  assert(desktop.HoldTime(Button::A) >= 0.8f);

  desktop.BeginFrame(0.016f);
  desktop.HandleEvent(ControllerButtonEvent(SDL_CONTROLLERBUTTONUP, SDL_CONTROLLER_BUTTON_A));
  desktop.EndFrame();
  assert(!desktop.IsPressed(Button::A));
  assert(desktop.IsJustReleased(Button::A));

  InputManager gkd(empty_map.string(), InputProfile::GKD350HUltra);
  gkd.BeginFrame(0.016f);
  gkd.HandleEvent(JoyButtonEvent(SDL_JOYBUTTONDOWN, 1));
  gkd.HandleEvent(ControllerButtonEvent(SDL_CONTROLLERBUTTONDOWN, SDL_CONTROLLER_BUTTON_A));
  gkd.EndFrame();
  assert(!gkd.AnyPressed());
  assert(gkd.DescribeJoyMap().find("joy.0=B") != std::string::npos);
  assert(gkd.DescribeJoyMap().find("joy.8=Menu") != std::string::npos);

  const std::filesystem::path full_map = test_dir / "full.ini";
  WriteFile(full_map, "# user mapping comment\nA=key:107\n");
  const std::vector<InputCalibrationMapping> mappings = {
      {Button::A, RawInputBinding{RawInputSource::Keyboard, 107, 0, {}, true}},
      {Button::B, RawInputBinding{RawInputSource::GameControllerButton, 0, 0, {}, true}},
      {Button::Left, RawInputBinding{RawInputSource::JoystickAxis, 2, -1, "test pad", true}},
      {Button::Up, RawInputBinding{RawInputSource::JoystickHat, 0, SDL_HAT_UP, "test pad", true}},
  };
  assert(SaveInputCalibrationMappings(full_map.string(), "test-device", mappings));
  std::ifstream saved(full_map);
  const std::string saved_text((std::istreambuf_iterator<char>(saved)), std::istreambuf_iterator<char>());
  assert(saved_text.find("# user mapping comment") != std::string::npos);
  assert(saved_text.find("# ROCreader calibrated keymap") != std::string::npos);
  assert(saved_text.find("# calibration_version=5") != std::string::npos);
  assert(saved_text.find("# calibration_model=test-device") != std::string::npos);
  assert(saved_text.find("joy_axis.2.neg=Left") != std::string::npos);
  InputManager mapped(full_map.string(), InputProfile::DesktopDefault);
  mapped.BeginFrame(0.016f);
  mapped.HandleEvent(KeyEvent(SDL_KEYDOWN, SDLK_k));
  mapped.HandleEvent(ControllerButtonEvent(SDL_CONTROLLERBUTTONDOWN, 0));
  mapped.HandleEvent(JoyAxisEvent(2, -20000));
  mapped.HandleEvent(JoyHatEvent(0, SDL_HAT_UP));
  mapped.EndFrame();
  assert(mapped.IsPressed(Button::A));
  assert(mapped.IsPressed(Button::B));
  assert(mapped.IsPressed(Button::Left));
  assert(mapped.IsPressed(Button::Up));

  const std::filesystem::path legacy_map = test_dir / "legacy.ini";
  WriteFile(legacy_map,
            "A=key:107\n"
            "B=pad:0\n"
            "Left=joy_axis:2:neg\n"
            "Up=joy_hat:0:up\n");
  InputManager legacy(legacy_map.string(), InputProfile::DesktopDefault);
  legacy.BeginFrame(0.016f);
  legacy.HandleEvent(KeyEvent(SDL_KEYDOWN, SDLK_k));
  legacy.HandleEvent(ControllerButtonEvent(SDL_CONTROLLERBUTTONDOWN, 0));
  legacy.HandleEvent(JoyAxisEvent(2, -20000));
  legacy.HandleEvent(JoyHatEvent(0, SDL_HAT_UP));
  legacy.EndFrame();
  assert(legacy.IsPressed(Button::A));
  assert(legacy.IsPressed(Button::B));
  assert(legacy.IsPressed(Button::Left));
  assert(legacy.IsPressed(Button::Up));

  desktop.ResetAll();
  desktop.ClearCalibrationSamples();
  desktop.BeginFrame(0.016f);
  desktop.HandleEvent(JoyAxisEvent(3, -20000));
  desktop.EndFrame();
  RawInputBinding sample;
  assert(desktop.TakeCalibrationSample(sample));
  assert(sample.source == RawInputSource::JoystickAxis);
  assert(sample.code == 3);
  assert(sample.direction == -1);
  assert(sample.pressed);
  sample.device_name = "test pad";
  assert(RawInputBindingKey(sample) == "joy_axis.3.neg");
  assert(DescribeRawInputBinding(sample).find("test pad") != std::string::npos);

  desktop.BeginFrame(0.016f);
  desktop.HandleEvent(JoyAxisEvent(3, 0));
  desktop.EndFrame();
  assert(desktop.TakeCalibrationSample(sample));
  assert(sample.direction == 0);
  assert(!sample.pressed);

  assert(mapped.ReloadMappings());
  std::cout << "input manager tests passed\n";
  return 0;
}

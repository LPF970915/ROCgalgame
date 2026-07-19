#include "app_services.h"

void CloseAppInputDevices(AppInputDevices &devices) {
  for (SDL_GameController *controller : devices.controllers) {
    if (controller) SDL_GameControllerClose(controller);
  }
  for (SDL_Joystick *joystick : devices.joysticks) {
    if (joystick) SDL_JoystickClose(joystick);
  }
  devices.controllers.clear();
  devices.joysticks.clear();
}

void OpenAppInputDevices(AppInputDevices &devices) {
  CloseAppInputDevices(devices);
  SDL_JoystickEventState(SDL_ENABLE);
  for (int i = 0; i < SDL_NumJoysticks(); ++i) {
    if (SDL_IsGameController(i)) {
      if (SDL_GameController *controller = SDL_GameControllerOpen(i)) devices.controllers.push_back(controller);
    } else if (SDL_Joystick *joystick = SDL_JoystickOpen(i)) {
      devices.joysticks.push_back(joystick);
    }
  }
}

void RefreshAppInputDevices(AppInputDevices &devices) { OpenAppInputDevices(devices); }

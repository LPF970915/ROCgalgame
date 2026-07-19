#pragma once

#include <SDL.h>

#include <vector>

struct AppInputDevices {
  std::vector<SDL_GameController *> controllers;
  std::vector<SDL_Joystick *> joysticks;
};

void OpenAppInputDevices(AppInputDevices &devices);
void CloseAppInputDevices(AppInputDevices &devices);
void RefreshAppInputDevices(AppInputDevices &devices);

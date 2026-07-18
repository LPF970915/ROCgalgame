#include <SDL.h>

#include <iostream>

int main() {
  if (SDL_Init(SDL_INIT_JOYSTICK | SDL_INIT_GAMECONTROLLER | SDL_INIT_EVENTS) != 0) {
    std::cerr << "SDL_Init failed: " << SDL_GetError() << "\n";
    return 1;
  }

  const int count = SDL_NumJoysticks();
  std::cout << "joysticks=" << count << "\n";
  for (int i = 0; i < count; ++i) {
    char guid[64]{};
    SDL_JoystickGetGUIDString(SDL_JoystickGetDeviceGUID(i), guid, sizeof(guid));
    const SDL_bool is_controller = SDL_IsGameController(i);
    std::cout << "index=" << i << " name="
              << (SDL_JoystickNameForIndex(i) ? SDL_JoystickNameForIndex(i) : "")
              << " guid=" << guid << " is_controller=" << (is_controller == SDL_TRUE ? 1 : 0) << "\n";
    if (is_controller == SDL_TRUE) {
      char *mapping = SDL_GameControllerMappingForDeviceIndex(i);
      std::cout << "mapping=" << (mapping ? mapping : "") << "\n";
      SDL_free(mapping);
    }
  }

  SDL_Quit();
  return 0;
}

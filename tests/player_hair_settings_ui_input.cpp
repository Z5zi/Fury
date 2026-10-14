#include "player_hair_settings_ui_input.hpp"
Uint8 fury_settings_test_keys[SDL_NUM_SCANCODES]{};
// Simulated SDL keyboard state at the production poll boundary. This is not a
// physical keyboard claim. Controller tests use SDL's real virtual device API.
extern "C" const Uint8* __wrap_SDL_GetKeyboardState(int* count) {
  if(count) *count=SDL_NUM_SCANCODES;
  return fury_settings_test_keys;
}

#include "fury/input.hpp"

#include <SDL.h>

namespace fury {

bool Input::poll(InputState& out) {
  out.escape_pressed = false;
  SDL_Event event;
  while (SDL_PollEvent(&event)) {
    if (event.type == SDL_QUIT) {
      out.quit_requested = true;
    } else if (event.type == SDL_KEYDOWN) {
      if (event.key.keysym.sym == SDLK_ESCAPE) {
        out.escape_pressed = true;
        out.quit_requested = true;
      }
    }
  }
  return !out.quit_requested;
}

}  // namespace fury

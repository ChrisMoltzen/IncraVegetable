// Grass.h - the tufts of grass scattered over the farm background. Each tuft is
// its own sprite (farm/grass_tuft) and sways in the breeze: the sprite is drawn
// as a small mesh whose top bends further than its base, like a vertex shader
// would do, with a gust that rolls across the field and tufts leaning away from
// the pointer.
#pragma once

#include <SDL3/SDL.h>

namespace grass {

// Draw every tuft. `seconds` drives the sway; (mouseX, mouseY) is the pointer in game
// coordinates (tufts near it get brushed aside). Pass a far-away pointer to ignore it.
void draw(SDL_Renderer* r, float seconds, float mouseX, float mouseY);

// The built-in tuft (used when assets/farm/grass_tuft.png is missing, and for its art template).
void drawTuftBuiltin(SDL_Renderer* r, const SDL_FRect& rc);

} // namespace grass

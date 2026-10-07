// CropLooks.h - the built-in drawings of crops, shared by the game and the
// Tech Tree Editor (so the editor's preview is exactly what the game draws).
//
// A crop's "look" is one of the shapes below, drawn in its colour. Artwork in
// assets/crops/<id>.png replaces the built-in look in the game.
#pragma once

#include <SDL3/SDL.h>
#include <string>
#include <vector>

namespace croplook {

// "lettuce", "carrot", "pumpkin", "round" (a round fruit: tomato, berry, onion...)
const std::vector<std::string>& looks();
bool isLook(const std::string& look);

// The look's usual colour (lettuce green, carrot orange...).
SDL_Color defaultColor(const std::string& look);

// "e05040" -> colour. Empty or invalid -> the look's default colour.
SDL_Color parseColor(const std::string& hex, const std::string& look);
bool isValidColor(const std::string& hex); // empty counts as valid (= default)
std::string toHex(SDL_Color c);

// A ripe crop centred on (cx, cy), filling a square of side `size`.
void draw(SDL_Renderer* r, const std::string& look, SDL_Color color, float cx, float cy, float size);
// The little sprout every growing crop starts as, in a square.
void drawSprout(SDL_Renderer* r, const SDL_FRect& rc);

} // namespace croplook

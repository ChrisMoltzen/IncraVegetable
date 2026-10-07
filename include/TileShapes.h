// TileShapes.h - the shapes a tech's tile can have on the tech tree, shared
// by the game and the Tech Tree Editor so both draw them the same way.
#pragma once

#include <SDL3/SDL.h>
#include <string>
#include <vector>

namespace tileshape {

// "square" (the default), "circle", "triangle", "pentagon"
const std::vector<std::string>& shapes();
bool isShape(const std::string& s); // "" counts (= square)

// The outline of `shape` filling `rc`, inset by `inset` pixels.
std::vector<SDL_FPoint> outline(const std::string& shape, const SDL_FRect& rc, float inset = 0.f);

// A tile: `border` thick edge in `borderColor`, filled with `fill`.
void draw(SDL_Renderer* r, const std::string& shape, const SDL_FRect& rc, SDL_Color fill, SDL_Color borderColor,
          float border = 3.f);
// Just the filled shape (for shadows, flashes, dimming).
void fill(SDL_Renderer* r, const std::string& shape, const SDL_FRect& rc, SDL_Color color, float inset = 0.f);

// Where the icon and the level bar go inside the shape (triangles are
// widest at the bottom, so their icon sits lower and smaller).
SDL_FRect iconRect(const std::string& shape, const SDL_FRect& rc, float iconSize);
SDL_FRect barRect(const std::string& shape, const SDL_FRect& rc);

// Hex colours like "4a7c3a". Empty or invalid -> false.
bool parseHex(const std::string& hex, SDL_Color& out);
bool isValidHex(const std::string& hex); // empty counts as valid (= the usual colour)

} // namespace tileshape

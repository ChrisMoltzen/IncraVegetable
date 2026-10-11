// FontLatin.h - accented letters for the pixel font (see FontLatin.cpp).
#pragma once

#include <SDL3/SDL.h>

namespace fontlatin {

// The 16 x 6 sheet of U+00A0..U+00FF: assets/ui/font_latin1.png, or one made
// from assets/ui/font.png. nullptr if there's no font image at all.
SDL_Texture* texture(SDL_Renderer* r);
void forget(); // after the art is reloaded (F5)

// The plain letter for an accented one (é -> e), for the built-in debug font.
char plain(unsigned codepoint);

// Draws the sheet (the art template for ui/font_latin1).
void drawTemplate(SDL_Renderer* r, const SDL_FRect& rc);

} // namespace fontlatin

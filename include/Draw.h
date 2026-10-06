// Draw.h - small immediate-mode drawing helpers built on SDL3's renderer.
// Everything is drawn with simple shapes and SDL's built-in debug font,
// so the game needs no image or font files.
#pragma once

#include <SDL3/SDL.h>
#include <string>
#include <vector>

namespace draw {

enum class Align { Left, Center, Right };

void fillRect(SDL_Renderer* r, float x, float y, float w, float h, SDL_Color c);
void outlineRect(SDL_Renderer* r, float x, float y, float w, float h, SDL_Color c, int thickness = 1);
void fillRoundRect(SDL_Renderer* r, float x, float y, float w, float h, float radius, SDL_Color c);
void fillEllipse(SDL_Renderer* r, float cx, float cy, float rx, float ry, SDL_Color c, int segments = 28);
void fillCircle(SDL_Renderer* r, float cx, float cy, float radius, SDL_Color c, int segments = 28);
// Filled shape whose outline can be seen from (cx, cy) - fine for blobs and stars.
void fillPolygon(SDL_Renderer* r, float cx, float cy, const std::vector<SDL_FPoint>& outline, SDL_Color c);
void circleOutline(SDL_Renderer* r, float cx, float cy, float radius, SDL_Color c);
void fillTriangle(SDL_Renderer* r, SDL_FPoint a, SDL_FPoint b, SDL_FPoint c, SDL_Color col);
void thickLine(SDL_Renderer* r, float x1, float y1, float x2, float y2, float thickness, SDL_Color c);

// Text uses SDL's 8x8 debug font, scaled up. Scale 2 = 16px tall glyphs.
void text(SDL_Renderer* r, float x, float y, const std::string& s, float scale, SDL_Color c,
          Align align = Align::Left);
// Same as text() but with a dark drop shadow, easier to read over busy backgrounds.
void textShadow(SDL_Renderer* r, float x, float y, const std::string& s, float scale, SDL_Color c,
                Align align = Align::Left);
float textWidth(const std::string& s, float scale);

// Splits text into lines of at most maxChars characters (on word boundaries).
std::vector<std::string> wrap(const std::string& s, size_t maxChars);

// printf-style formatting into a std::string.
std::string strf(const char* fmt, ...);

// Formats big numbers for an incremental game: 12.5, 940, 1.23K, 45.6M ...
std::string number(double v);

bool pointInRect(float px, float py, const SDL_FRect& rc);
SDL_Color withAlpha(SDL_Color c, Uint8 a);
SDL_Color lerp(SDL_Color a, SDL_Color b, float t);

} // namespace draw

// Gfx.h - simple shape and text drawing for the editor (SDL3 renderer + built-in font).
#pragma once

#include <SDL3/SDL.h>
#include <string>
#include <vector>

namespace gfx {

enum class Align { Left, Center, Right };

void fillRect(SDL_Renderer* r, const SDL_FRect& rc, SDL_Color c);
void outline(SDL_Renderer* r, const SDL_FRect& rc, SDL_Color c, float thickness = 1.f);
void fillRound(SDL_Renderer* r, const SDL_FRect& rc, float radius, SDL_Color c);
void fillCircle(SDL_Renderer* r, float cx, float cy, float radius, SDL_Color c);
void line(SDL_Renderer* r, float x1, float y1, float x2, float y2, float thickness, SDL_Color c);
void triangle(SDL_Renderer* r, SDL_FPoint a, SDL_FPoint b, SDL_FPoint c, SDL_Color col);

// Text with SDL's 8x8 font. Glyphs are 8 * scale pixels.
void text(SDL_Renderer* r, float x, float y, const std::string& s, float scale, SDL_Color c, Align a = Align::Left);
float textWidth(const std::string& s, float scale);
std::vector<std::string> wrap(const std::string& s, size_t maxChars);
// Cuts text to fit maxWidth, adding "..." if needed.
std::string fit(const std::string& s, float scale, float maxWidth);

bool inside(float x, float y, const SDL_FRect& rc);
SDL_Color alpha(SDL_Color c, Uint8 a);
std::string strf(const char* fmt, ...);
std::string number(double v); // 12.5, 940, 1.23K, 45.6M ...

// Colours used throughout the editor.
namespace col {
inline constexpr SDL_Color bg{22, 26, 25, 255};
inline constexpr SDL_Color grid{33, 40, 38, 255};
inline constexpr SDL_Color gridMajor{42, 51, 48, 255};
inline constexpr SDL_Color panel{30, 34, 38, 255};
inline constexpr SDL_Color panelLine{52, 58, 64, 255};
inline constexpr SDL_Color text{232, 234, 230, 255};
inline constexpr SDL_Color dim{150, 156, 160, 255};
inline constexpr SDL_Color faint{105, 110, 115, 255};
inline constexpr SDL_Color accent{240, 160, 50, 255};
inline constexpr SDL_Color good{120, 210, 110, 255};
inline constexpr SDL_Color bad{240, 100, 90, 255};
inline constexpr SDL_Color gold{255, 205, 70, 255};
inline constexpr SDL_Color field{16, 18, 21, 255};
} // namespace col

} // namespace gfx

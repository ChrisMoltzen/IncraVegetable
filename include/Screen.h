// Screen.h - fitting the game to screens that aren't 16:9.
//
// The game is laid out on a 1280 x 720 "stage". On desktop the stage is
// scaled to the window, with black bars if the shapes differ. On phones and
// tablets (kFillScreen) the screen is filled instead: the canvas grows to the
// screen's shape (wider on an iPhone, taller on an iPad), the stage sits in the
// middle of it, and the backgrounds, top bars and dimming reach every edge.
//
// Everything still draws in stage coordinates. Only code that wants to reach
// past the stage uses a screen::Whole, which draws in whole-screen coordinates
// for as long as it's alive (stage point x maps to x + ox there).
#pragma once

#include "Platform.h"

#include <SDL3/SDL.h>
#include <string>

// Phones and tablets fill the screen. (-DINCRA_FILL_SCREEN=1 tries it on desktop.)
#if defined(INCRA_FILL_SCREEN)
inline constexpr bool kFillScreen = INCRA_FILL_SCREEN != 0;
#else
inline constexpr bool kFillScreen = kIsMobile;
#endif

namespace screen {

inline constexpr int kStageW = 1280;
inline constexpr int kStageH = 720;

// Call at the start of every frame: sizes the canvas to the screen and makes
// drawing go to the stage. Does nothing while drawing into an image.
void fit(SDL_Renderer* r);

// The whole screen in stage coordinates (x and y are <= 0). The stage itself
// when the screen isn't being filled.
SDL_FRect bounds();

// While alive, draws go to the whole screen: (0, 0) is its top-left corner and
// a stage point (x, y) is at (x + ox, y + oy). When the screen isn't being
// filled (desktop, or drawing into an image) it changes nothing and ox = oy = 0.
class Whole {
public:
    explicit Whole(SDL_Renderer* r);
    ~Whole();
    Whole(const Whole&) = delete;
    Whole& operator=(const Whole&) = delete;
    float ox = 0.f, oy = 0.f; // where the stage's top-left corner is
    float w = kStageW, h = kStageH; // size of the whole screen
    SDL_FRect stage(const SDL_FRect& rc) const { return SDL_FRect{rc.x + ox, rc.y + oy, rc.w, rc.h}; }

private:
    SDL_Renderer* r_;
    bool active_ = false;
};

// A colour over the whole screen (dimming behind menus).
void fillAll(SDL_Renderer* r, SDL_Color c);

// A stage-sized background image, with its edge pixels stretched out to the
// edges of the screen.
void background(SDL_Renderer* r, const std::string& art);

// A strip along the top of the stage (the HUD and The Barn's header), stretched
// to the full width of the screen and reaching up to its top edge.
void topBar(SDL_Renderer* r, const std::string& art, float height);

} // namespace screen

// UI.h - buttons and sliders shared by the menus.
// Works with both mouse and touch (SDL turns taps into mouse events).
#pragma once

#include <SDL3/SDL.h>
#include <string>
#include <vector>

namespace ui {

enum class Style { Primary, Secondary, Danger, Ghost };

struct Button {
    SDL_FRect rect{};
    std::string label;
    std::string subLabel; // optional small second line
    bool enabled = true;
    Style style = Style::Primary;
    float textScale = 2.5f;
    float subScale = 1.25f;
};

enum class ButtonState { Normal, Hover, Pressed, Disabled };

// These draw the artist's image if there is one (see Art.h), else the built-in look.
void drawButton(SDL_Renderer* r, const Button& b, bool hovered, bool pressed);
void drawPanel(SDL_Renderer* r, const SDL_FRect& rc);
void drawPauseIcon(SDL_Renderer* r, const SDL_FRect& rc, bool hovered);
void drawCoin(SDL_Renderer* r, float cx, float cy, float radius);

// The day/night dial in the HUD (like a watch's moon-phase window): a sky
// disc turns clockwise behind a half-circle window, exactly half a turn over
// the day: the sun starts at the top, sets on the right halfway through, and
// when the timer runs out the moon is at the top.
// (cx, horizonY) is the middle of the horizon, R the window's radius,
// t how far through the day it is (0 = start, 1 = timer finished).
// Art: ui/dial_sky is the whole disc (day half on top with the sun at the
// top, night half below with the moon at the bottom), turned by the game;
// ui/dial_frame goes over it.
void drawDayDial(SDL_Renderer* r, float cx, float horizonY, float R, float t);
SDL_FRect dialFrameRect(float cx, float horizonY, float R);

// Built-in looks, used when there's no image (and for the art templates).
const char* artName(Style s); // "ui/button_primary" etc.
void drawButtonBuiltin(SDL_Renderer* r, const SDL_FRect& rc, Style style, ButtonState state);
void drawPanelBuiltin(SDL_Renderer* r, const SDL_FRect& rc);
void drawPauseIconBuiltin(SDL_Renderer* r, const SDL_FRect& rc, bool hovered);
void drawCoinBuiltin(SDL_Renderer* r, const SDL_FRect& rc);
void drawDialSkyBuiltin(SDL_Renderer* r, const SDL_FRect& rc);   // the whole disc, as in the template
void drawDialFrameBuiltin(SDL_Renderer* r, const SDL_FRect& rc); // rim, ticks, hills and base
void dim(SDL_Renderer* r, Uint8 alpha);

// A set of buttons that handles hover and press. A button fires when the
// mouse/finger is released over the same button it went down on.
class ButtonList {
public:
    std::vector<Button> buttons;

    // Returns the index of the button that was clicked, or -1.
    int handleEvent(const SDL_Event& e);
    void render(SDL_Renderer* r) const;
    void clearPress() { pressed_ = -1; }

private:
    int indexAt(float x, float y) const;
    float mouseX_ = -1000.f, mouseY_ = -1000.f;
    int pressed_ = -1;
    bool touch_ = false;
};

} // namespace ui

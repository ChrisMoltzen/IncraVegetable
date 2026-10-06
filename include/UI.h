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

// Built-in looks, used when there's no image (and for the art templates).
const char* artName(Style s); // "ui/button_primary" etc.
void drawButtonBuiltin(SDL_Renderer* r, const SDL_FRect& rc, Style style, ButtonState state);
void drawPanelBuiltin(SDL_Renderer* r, const SDL_FRect& rc);
void drawPauseIconBuiltin(SDL_Renderer* r, const SDL_FRect& rc, bool hovered);
void drawCoinBuiltin(SDL_Renderer* r, const SDL_FRect& rc);
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

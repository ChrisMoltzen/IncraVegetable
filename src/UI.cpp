#include "UI.h"

#include "Art.h"
#include "Draw.h"

#include <algorithm>

namespace ui {

namespace {
struct Palette {
    SDL_Color fill, hover, text;
};
Palette paletteFor(Style s) {
    switch (s) {
    case Style::Primary: return {{240, 160, 50, 255}, {255, 190, 80, 255}, {255, 255, 255, 255}};
    case Style::Secondary: return {{80, 170, 70, 255}, {110, 200, 90, 255}, {255, 255, 255, 255}};
    case Style::Danger: return {{205, 75, 60, 255}, {235, 100, 80, 255}, {255, 255, 255, 255}};
    case Style::Ghost: return {{60, 66, 72, 255}, {85, 92, 100, 255}, {235, 235, 235, 255}};
    }
    return {{240, 160, 50, 255}, {255, 190, 80, 255}, {255, 255, 255, 255}};
}
} // namespace

const char* artName(Style s) {
    switch (s) {
    case Style::Primary: return "ui/button_primary";
    case Style::Secondary: return "ui/button_secondary";
    case Style::Danger: return "ui/button_danger";
    case Style::Ghost: return "ui/button_ghost";
    }
    return "ui/button_primary";
}

void drawButtonBuiltin(SDL_Renderer* r, const SDL_FRect& rc, Style style, ButtonState state) {
    Palette p = paletteFor(style);
    bool disabled = state == ButtonState::Disabled;
    SDL_Color fill = disabled ? SDL_Color{120, 120, 115, 255} : (state == ButtonState::Normal ? p.fill : p.hover);
    draw::fillRoundRect(r, rc.x, rc.y, rc.w, rc.h, 14, fill);
    // A lighter strip along the top gives the button a little depth.
    draw::fillRoundRect(r, rc.x + 6, rc.y + 4, rc.w - 12, rc.h * 0.35f, 10,
                        draw::withAlpha(SDL_Color{255, 255, 255, 255}, disabled ? 15 : 35));
}

void drawButton(SDL_Renderer* r, const Button& b, bool hovered, bool pressed) {
    Palette p = paletteFor(b.style);
    SDL_Color text = b.enabled ? p.text : SDL_Color{190, 190, 185, 255};
    float press = (pressed && b.enabled) ? 3.f : 0.f;
    const SDL_FRect& rc = b.rect;
    const char* name = artName(b.style);
    const char* variant = !b.enabled ? "_disabled" : (pressed ? "_pressed" : (hovered ? "_hover" : ""));

    if (!art::has(name)) draw::fillRoundRect(r, rc.x + 3, rc.y + 6, rc.w, rc.h, 14, SDL_Color{0, 0, 0, 90}); // shadow
    art::drawVariant(r, name, variant, SDL_FRect{rc.x, rc.y + press, rc.w, rc.h});

    float cy = rc.y + rc.h * 0.5f + press;
    float glyph = 8.f * b.textScale;
    if (b.subLabel.empty()) {
        draw::textShadow(r, rc.x + rc.w * 0.5f, cy - glyph * 0.5f, b.label, b.textScale, text, draw::Align::Center);
    } else {
        float sub = 8.f * b.subScale;
        float top = cy - (glyph + 8.f + sub) * 0.5f;
        draw::textShadow(r, rc.x + rc.w * 0.5f, top, b.label, b.textScale, text, draw::Align::Center);
        draw::text(r, rc.x + rc.w * 0.5f, top + glyph + 8.f, b.subLabel, b.subScale,
                   draw::withAlpha(text, 210), draw::Align::Center);
    }
}

void drawPanelBuiltin(SDL_Renderer* r, const SDL_FRect& rc) {
    draw::fillRoundRect(r, rc.x, rc.y, rc.w, rc.h, 24, SDL_Color{210, 170, 110, 255});
    draw::fillRoundRect(r, rc.x + 3, rc.y + 3, rc.w - 6, rc.h - 6, 21, SDL_Color{250, 236, 205, 255});
}

void drawPanel(SDL_Renderer* r, const SDL_FRect& rc) {
    if (!art::has("ui/panel")) draw::fillRoundRect(r, rc.x + 4, rc.y + 8, rc.w, rc.h, 22, SDL_Color{0, 0, 0, 110});
    art::draw(r, "ui/panel", SDL_FRect{rc.x - 3, rc.y - 3, rc.w + 6, rc.h + 6});
}

void drawPauseIconBuiltin(SDL_Renderer* r, const SDL_FRect& rc, bool hovered) {
    draw::fillRoundRect(r, rc.x, rc.y, rc.w, rc.h, rc.w * 0.25f,
                        hovered ? SDL_Color{90, 100, 90, 230} : SDL_Color{50, 58, 50, 210});
    float bw = rc.w * 0.16f, bh = rc.h * 0.46f;
    float cx = rc.x + rc.w * 0.5f, cy = rc.y + rc.h * 0.5f;
    draw::fillRoundRect(r, cx - bw * 1.6f, cy - bh * 0.5f, bw, bh, 2, SDL_Color{240, 240, 230, 255});
    draw::fillRoundRect(r, cx + bw * 0.6f, cy - bh * 0.5f, bw, bh, 2, SDL_Color{240, 240, 230, 255});
}

void drawPauseIcon(SDL_Renderer* r, const SDL_FRect& rc, bool hovered) {
    art::drawVariant(r, "ui/pause_button", hovered ? "_hover" : "", rc);
}

void drawCoinBuiltin(SDL_Renderer* r, const SDL_FRect& rc) {
    float cx = rc.x + rc.w * 0.5f, cy = rc.y + rc.h * 0.5f, rad = std::min(rc.w, rc.h) * 0.5f;
    draw::fillCircle(r, cx, cy, rad, SDL_Color{255, 205, 70, 255});
    draw::fillCircle(r, cx, cy, rad * 0.6f, SDL_Color{230, 170, 40, 255});
}

void drawCoin(SDL_Renderer* r, float cx, float cy, float radius) {
    art::draw(r, "ui/coin", SDL_FRect{cx - radius, cy - radius, radius * 2, radius * 2});
}

void dim(SDL_Renderer* r, Uint8 alpha) { draw::fillRect(r, 0, 0, 1280, 720, SDL_Color{0, 0, 0, alpha}); }

int ButtonList::indexAt(float x, float y) const {
    for (int i = 0; i < static_cast<int>(buttons.size()); ++i) {
        if (buttons[i].enabled && draw::pointInRect(x, y, buttons[i].rect)) return i;
    }
    return -1;
}

int ButtonList::handleEvent(const SDL_Event& e) {
    switch (e.type) {
    case SDL_EVENT_MOUSE_MOTION:
        mouseX_ = e.motion.x;
        mouseY_ = e.motion.y;
        touch_ = e.motion.which == SDL_TOUCH_MOUSEID;
        break;
    case SDL_EVENT_MOUSE_BUTTON_DOWN:
        if (e.button.button != SDL_BUTTON_LEFT) break;
        mouseX_ = e.button.x;
        mouseY_ = e.button.y;
        touch_ = e.button.which == SDL_TOUCH_MOUSEID;
        pressed_ = indexAt(e.button.x, e.button.y);
        break;
    case SDL_EVENT_MOUSE_BUTTON_UP: {
        if (e.button.button != SDL_BUTTON_LEFT) break;
        int hit = indexAt(e.button.x, e.button.y);
        int was = pressed_;
        pressed_ = -1;
        if (touch_) mouseX_ = mouseY_ = -1000.f; // no hover highlight left behind after a tap
        if (hit >= 0 && hit == was) return hit;
        break;
    }
    default:
        break;
    }
    return -1;
}

void ButtonList::render(SDL_Renderer* r) const {
    int hovered = indexAt(mouseX_, mouseY_);
    for (int i = 0; i < static_cast<int>(buttons.size()); ++i) {
        drawButton(r, buttons[i], i == hovered, i == pressed_ && i == hovered);
    }
}

} // namespace ui

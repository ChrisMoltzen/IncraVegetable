#include "UI.h"

#include "Art.h"
#include "Draw.h"

#include <algorithm>
#include <cmath>

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

// ---------------------------------------------------------------------------
// Day/night dial
// ---------------------------------------------------------------------------

namespace {
constexpr float kDialPi = 3.14159265f;
constexpr float kDialArc = 0.88f; // share of the half-turn the disc makes, so the sun peeks over the hills at dawn and dusk
const SDL_Color kDawn{236, 172, 132, 255}, kNoon{112, 160, 204, 255}, kDusk{212, 118, 88, 255};
const SDL_Color kNight{40, 50, 84, 255}, kSun{244, 196, 88, 255}, kSunCore{252, 226, 140, 255};
const SDL_Color kMoon{226, 226, 210, 255};
// Star positions as (angle, distance from the middle) on the disc.
const float kStars[][2] = {{0.35f, 0.82f}, {0.62f, 0.55f}, {0.95f, 0.88f}, {1.3f, 0.42f}, {1.55f, 0.75f},
                           {1.9f, 0.6f},   {2.2f, 0.86f},  {2.5f, 0.38f},  {2.8f, 0.7f}};

SDL_Color skyAt(float t) {
    if (t < 0.12f) return draw::lerp(kDawn, kNoon, t / 0.12f);
    if (t < 0.78f) return kNoon;
    if (t < 0.95f) return draw::lerp(kNoon, kDusk, (t - 0.78f) / 0.17f);
    return draw::lerp(kDusk, kNight, (t - 0.95f) / 0.05f * 0.6f);
}

// Half a disc above (up = true) or below its middle line.
std::vector<SDL_FPoint> halfDisc(float cx, float cy, float rad, bool up) {
    std::vector<SDL_FPoint> pts;
    for (int i = 0; i <= 32; ++i) {
        float a = kDialPi * i / 32.f;
        pts.push_back({cx - std::cos(a) * rad, cy + (up ? -1.f : 1.f) * std::sin(a) * rad});
    }
    return pts;
}

void drawSun(SDL_Renderer* r, float x, float y, float rad) {
    for (int k = 0; k < 8; ++k) {
        float a = kDialPi * k / 4.f;
        draw::thickLine(r, x + std::cos(a) * rad * 1.25f, y + std::sin(a) * rad * 1.25f, x + std::cos(a) * rad * 1.7f,
                        y + std::sin(a) * rad * 1.7f, std::max(1.5f, rad * 0.18f), kSun);
    }
    draw::fillCircle(r, x, y, rad, kSun);
    draw::fillCircle(r, x - rad * 0.15f, y - rad * 0.15f, rad * 0.65f, kSunCore);
}

void drawMoon(SDL_Renderer* r, float x, float y, float rad, SDL_Color shade) {
    draw::fillCircle(r, x, y, rad, kMoon);
    draw::fillCircle(r, x + rad * 0.45f, y - rad * 0.25f, rad * 0.85f, shade); // crescent
}
} // namespace

SDL_FRect dialFrameRect(float cx, float horizonY, float R) {
    const float rim = R * 0.12f, base = R * 0.3f;
    return {cx - R - rim, horizonY - R - rim, 2.f * (R + rim), R + rim + base};
}

// Template: the whole disc, day on top (sun at the top) and night below (moon at the bottom).
void drawDialSkyBuiltin(SDL_Renderer* r, const SDL_FRect& rc) {
    float cx = rc.x + rc.w * 0.5f, cy = rc.y + rc.h * 0.5f, R = std::min(rc.w, rc.h) * 0.5f;
    draw::fillPolygon(r, cx, cy, halfDisc(cx, cy, R, true), kNoon);
    draw::fillPolygon(r, cx, cy, halfDisc(cx, cy, R, false), kNight);
    for (const auto& st : kStars)
        draw::fillCircle(r, cx + std::cos(st[0]) * st[1] * R, cy + std::sin(st[0]) * st[1] * R, R * 0.025f + 1.f,
                         SDL_Color{240, 236, 210, 255});
    drawSun(r, cx, cy - R * 0.66f, R * 0.17f);
    drawMoon(r, cx, cy + R * 0.66f, R * 0.15f, kNight);
}

void drawDialFrameBuiltin(SDL_Renderer* r, const SDL_FRect& rc) {
    const float R = rc.w / 2.24f, rim = R * 0.12f; // matches dialFrameRect
    const float cx = rc.x + rc.w * 0.5f, hy = rc.y + rim + R;
    const SDL_Color metal{168, 170, 160, 255}, metalDark{110, 114, 108, 255}, plate{206, 204, 190, 255};
    // Rim: a thick half-ring.
    std::vector<SDL_Vertex> v;
    std::vector<int> idx;
    auto fc = [](SDL_Color c) { return SDL_FColor{c.r / 255.f, c.g / 255.f, c.b / 255.f, c.a / 255.f}; };
    for (int i = 0; i <= 32; ++i) {
        float a = kDialPi * i / 32.f, ca = -std::cos(a), sa = -std::sin(a);
        v.push_back({{cx + ca * (R + rim), hy + sa * (R + rim)}, fc(metal), {0, 0}});
        v.push_back({{cx + ca * R, hy + sa * R}, fc(metal), {0, 0}});
        if (i > 0) {
            int b = 2 * i;
            idx.insert(idx.end(), {b - 2, b - 1, b, b - 1, b + 1, b});
        }
    }
    SDL_RenderGeometry(r, nullptr, v.data(), static_cast<int>(v.size()), idx.data(), static_cast<int>(idx.size()));
    // Hour ticks on the rim.
    for (int k = 1; k < 6; ++k) {
        float a = kDialPi * k / 6.f;
        float x = cx - std::cos(a) * (R + rim * 0.5f), y = hy - std::sin(a) * (R + rim * 0.5f);
        draw::fillCircle(r, x, y, std::max(1.2f, rim * 0.22f), metalDark);
    }
    // Rolling hills on the horizon, then the base plate the sun sinks behind.
    const SDL_Color hill{88, 120, 72, 255}, hillFar{118, 146, 96, 255};
    draw::fillEllipse(r, cx - R * 0.45f, hy, R * 0.55f, R * 0.16f, hillFar);
    draw::fillEllipse(r, cx + R * 0.4f, hy, R * 0.6f, R * 0.12f, hillFar);
    draw::fillEllipse(r, cx + R * 0.05f, hy + R * 0.02f, R * 0.5f, R * 0.1f, hill);
    draw::fillRoundRect(r, rc.x, hy, rc.w, rc.y + rc.h - hy, rim * 0.8f, plate);
    draw::fillRect(r, rc.x, hy, rc.w, std::max(1.f, rim * 0.25f), metalDark);
}

void drawDayDial(SDL_Renderer* r, float cx, float hy, float R, float t) {
    t = std::clamp(t, 0.f, 1.f);
    const float turn = (t - 0.5f) * kDialPi * kDialArc; // how far the disc has turned (clockwise)
    if (SDL_Texture* tex = art::texture("ui/dial_sky")) {
        // Map the turned disc image onto the half-circle window (no clipping needed).
        std::vector<SDL_Vertex> v;
        std::vector<int> idx;
        const float c = std::cos(-turn), s = std::sin(-turn);
        auto vert = [&](float dx, float dy) {
            float ux = dx * c - dy * s, uy = dx * s + dy * c; // turn back into the image
            return SDL_Vertex{{cx + dx, hy + dy}, {1, 1, 1, 1}, {0.5f + ux / (2.f * R), 0.5f + uy / (2.f * R)}};
        };
        v.push_back(vert(0, 0));
        for (int i = 0; i <= 40; ++i) {
            float a = kDialPi * i / 40.f;
            v.push_back(vert(-std::cos(a) * R, -std::sin(a) * R));
            if (i > 0) idx.insert(idx.end(), {0, i, i + 1});
        }
        SDL_RenderGeometry(r, tex, v.data(), static_cast<int>(v.size()), idx.data(), static_cast<int>(idx.size()));
    } else {
        // Built-in: sky that warms at dawn and dusk, sun and moon on opposite sides of the disc.
        SDL_Color sky = skyAt(t);
        draw::fillPolygon(r, cx, hy, halfDisc(cx, hy, R, true), sky);
        draw::fillEllipse(r, cx, hy, R * 0.98f, R * 0.32f, draw::withAlpha(SDL_Color{255, 226, 180, 255}, 70)); // horizon glow
        float night = std::clamp((t - 0.82f) / 0.18f, 0.f, 1.f);
        if (night > 0.f) {
            for (const auto& st : kStars) {
                float a = -st[0], d = st[1] * R; // stars on the night half, turning with the disc
                float x = cx + (std::cos(a + turn) * d), y = hy + std::sin(a + turn) * d;
                if (y < hy - 2.f)
                    draw::fillCircle(r, x, y, R * 0.025f + 0.8f, draw::withAlpha(SDL_Color{250, 246, 220, 255}, static_cast<Uint8>(255 * night)));
            }
        }
        // Only what's above (or just sinking below) the horizon: the frame's base plate hides the rest.
        const float sunA = kDialPi * 0.5f - turn; // left at sunrise, right at sunset
        const float sunY = hy - std::sin(sunA) * R * 0.66f;
        if (sunY < hy + R * 0.02f) drawSun(r, cx + std::cos(sunA) * R * 0.66f, sunY, R * 0.17f);
        const float moonA = sunA + kDialPi;
        const float moonY = hy - std::sin(moonA) * R * 0.66f;
        if (moonY < hy + R * 0.12f) drawMoon(r, cx + std::cos(moonA) * R * 0.66f, moonY, R * 0.15f, sky);
    }
    art::draw(r, "ui/dial_frame", dialFrameRect(cx, hy, R));
}

} // namespace ui


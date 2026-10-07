#include "CropLooks.h"

#include <algorithm>
#include <cctype>
#include <cmath>
#include <cstdio>
#include <cstdlib>

namespace croplook {

namespace {

// Small drawing helpers that need nothing but SDL (so the editor can use this file too).
SDL_FColor fc(SDL_Color c) { return SDL_FColor{c.r / 255.f, c.g / 255.f, c.b / 255.f, c.a / 255.f}; }

void ellipse(SDL_Renderer* r, float cx, float cy, float rx, float ry, SDL_Color c, int segments = 28) {
    SDL_Vertex v[64 + 1];
    int idx[64 * 3];
    segments = std::clamp(segments, 3, 64);
    SDL_FColor col = fc(c);
    v[0] = {{cx, cy}, col, {0, 0}};
    for (int i = 0; i < segments; ++i) {
        float a = 6.2831853f * i / segments;
        v[1 + i] = {{cx + std::cos(a) * rx, cy + std::sin(a) * ry}, col, {0, 0}};
        idx[i * 3] = 0;
        idx[i * 3 + 1] = 1 + i;
        idx[i * 3 + 2] = 1 + (i + 1) % segments;
    }
    SDL_RenderGeometry(r, nullptr, v, segments + 1, idx, segments * 3);
}

void circle(SDL_Renderer* r, float cx, float cy, float rad, SDL_Color c) { ellipse(r, cx, cy, rad, rad, c); }

void triangle(SDL_Renderer* r, SDL_FPoint a, SDL_FPoint b, SDL_FPoint c, SDL_Color col) {
    SDL_FColor f = fc(col);
    SDL_Vertex v[3] = {{a, f, {0, 0}}, {b, f, {0, 0}}, {c, f, {0, 0}}};
    SDL_RenderGeometry(r, nullptr, v, 3, nullptr, 0);
}

void line(SDL_Renderer* r, float x1, float y1, float x2, float y2, float t, SDL_Color c) {
    float dx = x2 - x1, dy = y2 - y1, len = std::sqrt(dx * dx + dy * dy);
    if (len < 0.01f) return;
    float nx = -dy / len * t * 0.5f, ny = dx / len * t * 0.5f;
    SDL_FColor f = fc(c);
    SDL_Vertex v[4] = {{{x1 + nx, y1 + ny}, f, {0, 0}},
                       {{x1 - nx, y1 - ny}, f, {0, 0}},
                       {{x2 + nx, y2 + ny}, f, {0, 0}},
                       {{x2 - nx, y2 - ny}, f, {0, 0}}};
    int idx[6] = {0, 1, 2, 2, 1, 3};
    SDL_RenderGeometry(r, nullptr, v, 4, idx, 6);
}

void rect(SDL_Renderer* r, float x, float y, float w, float h, SDL_Color c) {
    SDL_SetRenderDrawColor(r, c.r, c.g, c.b, c.a);
    SDL_FRect rc{x, y, w, h};
    SDL_RenderFillRect(r, &rc);
}

SDL_Color mix(SDL_Color a, SDL_Color b, float t) {
    auto m = [t](Uint8 x, Uint8 y) { return static_cast<Uint8>(std::lround(x + (y - x) * t)); };
    return {m(a.r, b.r), m(a.g, b.g), m(a.b, b.b), 255};
}
SDL_Color darker(SDL_Color c, float t = 0.3f) { return mix(c, SDL_Color{0, 0, 0, 255}, t); }
SDL_Color lighter(SDL_Color c, float t = 0.35f) { return mix(c, SDL_Color{255, 255, 255, 255}, t); }
bool same(SDL_Color a, SDL_Color b) { return a.r == b.r && a.g == b.g && a.b == b.b; }

const SDL_Color kLeaf{70, 160, 60, 255};
const SDL_Color kLeafDark{44, 120, 44, 255};
const SDL_Color kLeafLight{140, 210, 100, 255};
const SDL_Color kOrange{240, 130, 30, 255};
const SDL_Color kOrangeDark{200, 95, 20, 255};
const SDL_Color kPumpkin{235, 120, 25, 255};
const SDL_Color kPumpkinDark{190, 85, 15, 255};
const SDL_Color kPumpkinLight{250, 145, 45, 255};
const SDL_Color kRound{214, 64, 52, 255};
const SDL_Color kStem{110, 80, 30, 255};

} // namespace

const std::vector<std::string>& looks() {
    static const std::vector<std::string> list = {"lettuce", "carrot", "pumpkin", "round"};
    return list;
}

bool isLook(const std::string& look) {
    for (const auto& l : looks())
        if (l == look) return true;
    return false;
}

SDL_Color defaultColor(const std::string& look) {
    if (look == "lettuce") return kLeaf;
    if (look == "carrot") return kOrange;
    if (look == "pumpkin") return kPumpkin;
    return kRound;
}

bool isValidColor(const std::string& hex) {
    if (hex.empty()) return true;
    if (hex.size() != 6) return false;
    for (char c : hex)
        if (!std::isxdigit(static_cast<unsigned char>(c))) return false;
    return true;
}

SDL_Color parseColor(const std::string& hex, const std::string& look) {
    if (hex.empty() || !isValidColor(hex)) return defaultColor(look);
    unsigned v = static_cast<unsigned>(std::strtoul(hex.c_str(), nullptr, 16));
    return SDL_Color{static_cast<Uint8>(v >> 16), static_cast<Uint8>(v >> 8), static_cast<Uint8>(v), 255};
}

std::string toHex(SDL_Color c) {
    char buf[8];
    std::snprintf(buf, sizeof(buf), "%02x%02x%02x", c.r, c.g, c.b);
    return buf;
}

void drawSprout(SDL_Renderer* r, const SDL_FRect& rc) {
    float s = rc.w, cx = rc.x + rc.w * 0.5f;
    float h = s * 0.32f;
    float base = rc.y + rc.h * 0.68f;
    line(r, cx, base, cx, base - h, std::max(2.f, s * 0.04f), kLeafDark);
    ellipse(r, cx - h * 0.45f, base - h * 0.85f, h * 0.45f, h * 0.22f, kLeaf);
    ellipse(r, cx + h * 0.45f, base - h * 0.95f, h * 0.45f, h * 0.22f, kLeafLight);
}

void draw(SDL_Renderer* r, const std::string& look, SDL_Color c, float cx, float cy, float s) {
    if (look == "lettuce") {
        // Shades of the crop's colour (the usual green keeps its hand-picked shades).
        bool usual = same(c, kLeaf);
        SDL_Color dark = usual ? kLeafDark : darker(c), light = usual ? kLeafLight : lighter(c);
        circle(r, cx, cy + s * 0.04f, s * 0.30f, dark);
        circle(r, cx - s * 0.12f, cy, s * 0.18f, c);
        circle(r, cx + s * 0.12f, cy, s * 0.18f, c);
        circle(r, cx, cy - s * 0.08f, s * 0.18f, c);
        circle(r, cx, cy + s * 0.01f, s * 0.11f, light);
    } else if (look == "carrot") {
        bool usual = same(c, kOrange);
        SDL_Color dark = usual ? kOrangeDark : darker(c, 0.2f);
        float w = std::max(2.f, s * 0.026f);
        // Leafy top...
        ellipse(r, cx - s * 0.10f, cy - s * 0.24f, s * 0.06f, s * 0.16f, kLeafDark);
        ellipse(r, cx + s * 0.10f, cy - s * 0.24f, s * 0.06f, s * 0.16f, kLeafDark);
        ellipse(r, cx, cy - s * 0.28f, s * 0.06f, s * 0.18f, kLeaf);
        // ...and the root.
        triangle(r, {cx - s * 0.15f, cy - s * 0.10f}, {cx + s * 0.15f, cy - s * 0.10f}, {cx, cy + s * 0.34f}, c);
        ellipse(r, cx, cy - s * 0.10f, s * 0.15f, s * 0.06f, c);
        line(r, cx - s * 0.08f, cy + s * 0.02f, cx - s * 0.01f, cy + s * 0.02f, w, dark);
        line(r, cx + s * 0.02f, cy + s * 0.12f, cx + s * 0.07f, cy + s * 0.12f, w, dark);
    } else if (look == "pumpkin") {
        bool usual = same(c, kPumpkin);
        SDL_Color dark = usual ? kPumpkinDark : darker(c, 0.2f), light = usual ? kPumpkinLight : lighter(c, 0.15f);
        ellipse(r, cx, cy + s * 0.06f, s * 0.36f, s * 0.27f, dark);
        ellipse(r, cx - s * 0.14f, cy + s * 0.06f, s * 0.17f, s * 0.25f, c);
        ellipse(r, cx + s * 0.14f, cy + s * 0.06f, s * 0.17f, s * 0.25f, c);
        ellipse(r, cx, cy + s * 0.06f, s * 0.13f, s * 0.27f, light);
        rect(r, cx - s * 0.03f, cy - s * 0.30f, s * 0.06f, s * 0.12f, kStem);
        ellipse(r, cx + s * 0.11f, cy - s * 0.24f, s * 0.09f, s * 0.04f, kLeaf);
    } else {
        // Round fruit: a shaded ball with a stalk and a leaf.
        circle(r, cx, cy + s * 0.06f, s * 0.27f, darker(c, 0.22f));
        circle(r, cx - s * 0.02f, cy + s * 0.04f, s * 0.24f, c);
        circle(r, cx - s * 0.10f, cy - s * 0.04f, s * 0.08f, lighter(c, 0.45f));
        line(r, cx, cy - s * 0.18f, cx + s * 0.03f, cy - s * 0.30f, std::max(2.f, s * 0.04f), kStem);
        ellipse(r, cx + s * 0.11f, cy - s * 0.24f, s * 0.10f, s * 0.045f, kLeaf);
    }
}

} // namespace croplook

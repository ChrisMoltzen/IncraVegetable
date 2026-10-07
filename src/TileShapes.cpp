#include "TileShapes.h"

#include <algorithm>
#include <cctype>
#include <cmath>
#include <cstdlib>

namespace tileshape {

namespace {
constexpr float kPi = 3.14159265f;

// A regular polygon with rounded corners: n corners on a circle of radius R
// around (cx, cy), the first at angle `start`, each corner rounded by `round`.
std::vector<SDL_FPoint> roundedPolygon(float cx, float cy, float R, int n, float start, float round) {
    std::vector<SDL_FPoint> pts;
    const float half = kPi / n;                         // half the angle between corners
    const float inner = R * std::cos(half);             // centre to edge
    round = std::min(round, inner * 0.9f);
    const float d = round / std::sin(kPi / 2 - half);   // centre of the rounding arc, back from the corner
    for (int k = 0; k < n; ++k) {
        float a = start + 2 * kPi * k / n;
        float ccx = cx + std::cos(a) * (R - d), ccy = cy + std::sin(a) * (R - d);
        for (int s = 0; s <= 4; ++s) {
            float b = a - half + 2.f * half * s / 4.f; // from the previous edge's direction to the next one's
            pts.push_back({ccx + std::cos(b) * round, ccy + std::sin(b) * round});
        }
    }
    return pts;
}

void fan(SDL_Renderer* r, const std::vector<SDL_FPoint>& pts, SDL_Color c) {
    if (pts.size() < 3) return;
    float cx = 0, cy = 0;
    for (auto p : pts) { cx += p.x; cy += p.y; }
    cx /= pts.size();
    cy /= pts.size();
    SDL_FColor f{c.r / 255.f, c.g / 255.f, c.b / 255.f, c.a / 255.f};
    std::vector<SDL_Vertex> v;
    std::vector<int> idx;
    v.push_back({{cx, cy}, f, {0, 0}});
    for (auto p : pts) v.push_back({p, f, {0, 0}});
    const int n = static_cast<int>(pts.size());
    for (int i = 0; i < n; ++i) idx.insert(idx.end(), {0, 1 + i, 1 + (i + 1) % n});
    SDL_RenderGeometry(r, nullptr, v.data(), static_cast<int>(v.size()), idx.data(), static_cast<int>(idx.size()));
}
} // namespace

const std::vector<std::string>& shapes() {
    static const std::vector<std::string> list = {"square", "circle", "triangle", "pentagon"};
    return list;
}

bool isShape(const std::string& s) {
    if (s.empty()) return true;
    for (const auto& x : shapes())
        if (x == s) return true;
    return false;
}

std::vector<SDL_FPoint> outline(const std::string& shape, const SDL_FRect& rc0, float inset) {
    SDL_FRect rc{rc0.x + inset, rc0.y + inset, rc0.w - 2 * inset, rc0.h - 2 * inset};
    float cx = rc.x + rc.w / 2, cy = rc.y + rc.h / 2, s = std::min(rc.w, rc.h);
    float round = s * 0.16f;
    if (shape == "circle") {
        std::vector<SDL_FPoint> pts;
        for (int i = 0; i < 40; ++i) {
            float a = 2 * kPi * i / 40;
            pts.push_back({cx + std::cos(a) * s / 2, cy + std::sin(a) * s / 2});
        }
        return pts;
    }
    if (shape == "triangle") {
        // Point up; sized to fill the box: height = s, so circumradius = s * 2/3, centre 1/6 below the middle.
        float R = s * 0.68f;
        return roundedPolygon(cx, cy + s * 0.12f, R, 3, -kPi / 2, round * 0.8f);
    }
    if (shape == "pentagon") {
        float R = s * 0.53f;
        return roundedPolygon(cx, cy + s * 0.04f, R, 5, -kPi / 2, round * 0.9f);
    }
    // Square: a rounded rectangle filling the box.
    std::vector<SDL_FPoint> pts;
    float rr = std::min(round, s / 2);
    const float cxs[4] = {rc.x + rc.w - rr, rc.x + rc.w - rr, rc.x + rr, rc.x + rr};
    const float cys[4] = {rc.y + rr, rc.y + rc.h - rr, rc.y + rc.h - rr, rc.y + rr};
    for (int k = 0; k < 4; ++k)
        for (int i = 0; i <= 5; ++i) {
            float a = -kPi / 2 + k * kPi / 2 + (kPi / 2) * i / 5;
            pts.push_back({cxs[k] + std::cos(a) * rr, cys[k] + std::sin(a) * rr});
        }
    return pts;
}

void fill(SDL_Renderer* r, const std::string& shape, const SDL_FRect& rc, SDL_Color color, float inset) {
    fan(r, outline(shape, rc, inset), color);
}

void draw(SDL_Renderer* r, const std::string& shape, const SDL_FRect& rc, SDL_Color fillColor, SDL_Color borderColor,
          float border) {
    fan(r, outline(shape, rc, 0.f), borderColor);
    fan(r, outline(shape, rc, border), fillColor);
}

SDL_FRect iconRect(const std::string& shape, const SDL_FRect& rc, float size) {
    float cx = rc.x + rc.w / 2, cy = rc.y + rc.h / 2;
    if (shape == "triangle") {
        size *= 0.62f;
        cy += rc.h * 0.14f;
    } else if (shape == "pentagon") {
        size *= 0.74f;
        cy += rc.h * 0.06f;
    } else if (shape == "circle") {
        size *= 0.9f;
    }
    cy -= 3.f; // room for the level bar
    return {cx - size / 2, cy - size / 2, size, size};
}

SDL_FRect barRect(const std::string& shape, const SDL_FRect& rc) {
    float w = rc.w - 20.f, y = rc.y + rc.h - 10.f;
    if (shape == "circle") {
        w = rc.w * 0.5f;
        y = rc.y + rc.h - 14.f;
    } else if (shape == "triangle") {
        w = rc.w * 0.6f;
        y = rc.y + rc.h - 12.f;
    } else if (shape == "pentagon") {
        w = rc.w * 0.52f;
        y = rc.y + rc.h - 11.f;
    }
    return {rc.x + (rc.w - w) / 2, y, w, 4.f};
}

bool isValidHex(const std::string& hex) {
    if (hex.empty()) return true;
    if (hex.size() != 6) return false;
    for (char c : hex)
        if (!std::isxdigit(static_cast<unsigned char>(c))) return false;
    return true;
}

bool parseHex(const std::string& hex, SDL_Color& out) {
    if (hex.empty() || !isValidHex(hex)) return false;
    unsigned v = static_cast<unsigned>(std::strtoul(hex.c_str(), nullptr, 16));
    out = SDL_Color{static_cast<Uint8>(v >> 16), static_cast<Uint8>(v >> 8), static_cast<Uint8>(v), 255};
    return true;
}

} // namespace tileshape

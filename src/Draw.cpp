#include "Draw.h"

#include "Art.h"

#include <algorithm>
#include <cmath>
#include <cstdarg>
#include <cstdio>
#include <sstream>

namespace draw {

static constexpr float PI = 3.14159265358979f;

static SDL_FColor toF(SDL_Color c) {
    return SDL_FColor{c.r / 255.f, c.g / 255.f, c.b / 255.f, c.a / 255.f};
}

static void setColor(SDL_Renderer* r, SDL_Color c) {
    SDL_SetRenderDrawColor(r, c.r, c.g, c.b, c.a);
}

void fillRect(SDL_Renderer* r, float x, float y, float w, float h, SDL_Color c) {
    setColor(r, c);
    SDL_FRect rc{x, y, w, h};
    SDL_RenderFillRect(r, &rc);
}

void outlineRect(SDL_Renderer* r, float x, float y, float w, float h, SDL_Color c, int thickness) {
    setColor(r, c);
    for (int i = 0; i < thickness; ++i) {
        SDL_FRect rc{x + i, y + i, w - 2.f * i, h - 2.f * i};
        SDL_RenderRect(r, &rc);
    }
}

// Draws a convex polygon as a triangle fan around (cx, cy).
static void fillFan(SDL_Renderer* r, float cx, float cy, const std::vector<SDL_FPoint>& rim, SDL_Color c) {
    if (rim.size() < 2) return;
    SDL_FColor fc = toF(c);
    std::vector<SDL_Vertex> verts;
    verts.reserve(rim.size() + 1);
    verts.push_back(SDL_Vertex{{cx, cy}, fc, {0, 0}});
    for (const auto& p : rim) verts.push_back(SDL_Vertex{p, fc, {0, 0}});
    std::vector<int> idx;
    idx.reserve(rim.size() * 3);
    int n = static_cast<int>(rim.size());
    for (int i = 0; i < n; ++i) {
        idx.push_back(0);
        idx.push_back(1 + i);
        idx.push_back(1 + (i + 1) % n);
    }
    SDL_RenderGeometry(r, nullptr, verts.data(), static_cast<int>(verts.size()), idx.data(),
                       static_cast<int>(idx.size()));
}

void fillEllipse(SDL_Renderer* r, float cx, float cy, float rx, float ry, SDL_Color c, int segments) {
    std::vector<SDL_FPoint> rim;
    rim.reserve(segments);
    for (int i = 0; i < segments; ++i) {
        float a = (2.f * PI * i) / segments;
        rim.push_back({cx + std::cos(a) * rx, cy + std::sin(a) * ry});
    }
    fillFan(r, cx, cy, rim, c);
}

void fillCircle(SDL_Renderer* r, float cx, float cy, float radius, SDL_Color c, int segments) {
    fillEllipse(r, cx, cy, radius, radius, c, segments);
}

void fillRoundRect(SDL_Renderer* r, float x, float y, float w, float h, float radius, SDL_Color c) {
    radius = std::max(0.f, std::min({radius, w * 0.5f, h * 0.5f}));
    if (radius < 1.f) {
        fillRect(r, x, y, w, h, c);
        return;
    }
    const int seg = 8;
    std::vector<SDL_FPoint> rim;
    rim.reserve(4 * (seg + 1));
    // Corner centres, going clockwise starting top-right.
    const SDL_FPoint centres[4] = {
        {x + w - radius, y + radius},     // top-right
        {x + w - radius, y + h - radius}, // bottom-right
        {x + radius, y + h - radius},     // bottom-left
        {x + radius, y + radius},         // top-left
    };
    const float startAngles[4] = {-PI / 2, 0, PI / 2, PI};
    for (int corner = 0; corner < 4; ++corner) {
        for (int i = 0; i <= seg; ++i) {
            float a = startAngles[corner] + (PI / 2) * i / seg;
            rim.push_back({centres[corner].x + std::cos(a) * radius, centres[corner].y + std::sin(a) * radius});
        }
    }
    fillFan(r, x + w * 0.5f, y + h * 0.5f, rim, c);
}

void fillPolygon(SDL_Renderer* r, float cx, float cy, const std::vector<SDL_FPoint>& outline, SDL_Color c) {
    fillFan(r, cx, cy, outline, c);
}

void circleOutline(SDL_Renderer* r, float cx, float cy, float radius, SDL_Color c) {
    SDL_FPoint pts[41];
    for (int i = 0; i <= 40; ++i) {
        float a = 2.f * PI * i / 40.f;
        pts[i] = {cx + std::cos(a) * radius, cy + std::sin(a) * radius};
    }
    setColor(r, c);
    SDL_RenderLines(r, pts, 41);
}

void fillTriangle(SDL_Renderer* r, SDL_FPoint a, SDL_FPoint b, SDL_FPoint c, SDL_Color col) {
    SDL_FColor fc = toF(col);
    SDL_Vertex v[3] = {{a, fc, {0, 0}}, {b, fc, {0, 0}}, {c, fc, {0, 0}}};
    SDL_RenderGeometry(r, nullptr, v, 3, nullptr, 0);
}

void thickLine(SDL_Renderer* r, float x1, float y1, float x2, float y2, float thickness, SDL_Color c) {
    float dx = x2 - x1, dy = y2 - y1;
    float len = std::sqrt(dx * dx + dy * dy);
    if (len < 0.001f) return;
    float nx = -dy / len * thickness * 0.5f;
    float ny = dx / len * thickness * 0.5f;
    SDL_FColor fc = toF(c);
    SDL_Vertex v[4] = {
        {{x1 + nx, y1 + ny}, fc, {0, 0}},
        {{x1 - nx, y1 - ny}, fc, {0, 0}},
        {{x2 + nx, y2 + ny}, fc, {0, 0}},
        {{x2 - nx, y2 - ny}, fc, {0, 0}},
    };
    int idx[6] = {0, 1, 2, 2, 1, 3};
    SDL_RenderGeometry(r, nullptr, v, 4, idx, 6);
}

// An artist-supplied font: assets/ui/font.png, a grid of 16 x 6 characters
// (ASCII 32 to 127, in order, left to right then top to bottom). All
// characters are the same width. Text height is always 8 x scale pixels.
static SDL_Texture* customFont(float& cellW, float& cellH) {
    SDL_Texture* tex = art::texture("ui/font");
    if (!tex) return nullptr;
    float w = 0, h = 0;
    SDL_GetTextureSize(tex, &w, &h);
    cellW = w / 16.f;
    cellH = h / 6.f;
    return cellW > 0 && cellH > 0 ? tex : nullptr;
}

float textWidth(const std::string& s, float scale) {
    float cw = 0, ch = 0;
    if (customFont(cw, ch)) return static_cast<float>(s.size()) * 8.f * scale * (cw / ch);
    return static_cast<float>(s.size()) * SDL_DEBUG_TEXT_FONT_CHARACTER_SIZE * scale;
}

void text(SDL_Renderer* r, float x, float y, const std::string& s, float scale, SDL_Color c, Align align) {
    float w = textWidth(s, scale);
    if (align == Align::Center) x -= w * 0.5f;
    else if (align == Align::Right) x -= w;

    float cw = 0, ch = 0;
    if (SDL_Texture* font = customFont(cw, ch)) {
        float h = 8.f * scale, adv = h * (cw / ch);
        SDL_SetTextureColorMod(font, c.r, c.g, c.b);
        SDL_SetTextureAlphaMod(font, c.a);
        for (unsigned char chr : s) {
            if (chr > 32 && chr < 128) {
                int i = chr - 32;
                SDL_FRect src{(i % 16) * cw, (i / 16) * ch, cw, ch};
                SDL_FRect dst{x, y, adv, h};
                SDL_RenderTexture(r, font, &src, &dst);
            }
            x += adv;
        }
        SDL_SetTextureColorMod(font, 255, 255, 255);
        SDL_SetTextureAlphaMod(font, 255);
        return;
    }
    setColor(r, c);
    SDL_SetRenderScale(r, scale, scale);
    SDL_RenderDebugText(r, x / scale, y / scale, s.c_str());
    SDL_SetRenderScale(r, 1.f, 1.f);
}

void textShadow(SDL_Renderer* r, float x, float y, const std::string& s, float scale, SDL_Color c, Align align) {
    float off = std::max(1.f, scale * 0.75f);
    text(r, x + off, y + off, s, scale, SDL_Color{0, 0, 0, static_cast<Uint8>(c.a * 0.6f)}, align);
    text(r, x, y, s, scale, c, align);
}

std::vector<std::string> wrap(const std::string& s, size_t maxChars) {
    std::vector<std::string> lines;
    std::istringstream in(s);
    std::string word, line;
    while (in >> word) {
        if (!line.empty() && line.size() + 1 + word.size() > maxChars) {
            lines.push_back(line);
            line.clear();
        }
        if (!line.empty()) line += ' ';
        line += word;
    }
    if (!line.empty()) lines.push_back(line);
    return lines;
}

std::string strf(const char* fmt, ...) {
    char buf[512];
    va_list args;
    va_start(args, fmt);
    std::vsnprintf(buf, sizeof(buf), fmt, args);
    va_end(args);
    return buf;
}

std::string number(double v) {
    static const char* suffixes[] = {"", "K", "M", "B", "T", "Qa", "Qi", "Sx", "Sp", "Oc", "No", "Dc"};
    if (v < 0) return "-" + number(-v);
    if (v < 100.0) {
        std::string s = strf("%.1f", std::floor(v * 10.0) / 10.0);
        if (s.size() > 2 && s.substr(s.size() - 2) == ".0") s.resize(s.size() - 2);
        return s;
    }
    if (v < 1000.0) return strf("%.0f", std::floor(v));
    int tier = 0;
    while (v >= 1000.0 && tier < 11) {
        v /= 1000.0;
        ++tier;
    }
    if (v >= 100.0) return strf("%.0f%s", v, suffixes[tier]);
    if (v >= 10.0) return strf("%.1f%s", v, suffixes[tier]);
    return strf("%.2f%s", v, suffixes[tier]);
}

bool pointInRect(float px, float py, const SDL_FRect& rc) {
    return px >= rc.x && py >= rc.y && px < rc.x + rc.w && py < rc.y + rc.h;
}

SDL_Color withAlpha(SDL_Color c, Uint8 a) {
    c.a = a;
    return c;
}

SDL_Color lerp(SDL_Color a, SDL_Color b, float t) {
    t = std::clamp(t, 0.f, 1.f);
    auto mix = [t](Uint8 x, Uint8 y) { return static_cast<Uint8>(x + (y - x) * t); };
    return SDL_Color{mix(a.r, b.r), mix(a.g, b.g), mix(a.b, b.b), mix(a.a, b.a)};
}

} // namespace draw

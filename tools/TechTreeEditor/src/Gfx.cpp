#include "Gfx.h"

#include <algorithm>
#include <cmath>
#include <cstdarg>
#include <cstdio>
#include <sstream>

// stb is third-party code; don't let its style trip our warning flags.
#if defined(__GNUC__)
#pragma GCC diagnostic push
#pragma GCC diagnostic ignored "-Wunused-function"
#pragma GCC diagnostic ignored "-Wmissing-field-initializers"
#pragma GCC diagnostic ignored "-Wsign-compare"
#pragma GCC diagnostic ignored "-Wunused-parameter"
#pragma GCC diagnostic ignored "-Wdeprecated-declarations"
#elif defined(_MSC_VER)
#pragma warning(push, 0)
#endif
#define STB_IMAGE_IMPLEMENTATION
#define STBI_ONLY_PNG
#define STBI_ONLY_JPEG
#define STBI_ONLY_BMP
#define STBI_ONLY_TGA
#include "stb_image.h"
#if defined(__GNUC__)
#pragma GCC diagnostic pop
#elif defined(_MSC_VER)
#pragma warning(pop)
#endif

namespace gfx {

static SDL_FColor f(SDL_Color c) { return SDL_FColor{c.r / 255.f, c.g / 255.f, c.b / 255.f, c.a / 255.f}; }

void fillRect(SDL_Renderer* r, const SDL_FRect& rc, SDL_Color c) {
    SDL_SetRenderDrawColor(r, c.r, c.g, c.b, c.a);
    SDL_RenderFillRect(r, &rc);
}

void outline(SDL_Renderer* r, const SDL_FRect& rc, SDL_Color c, float t) {
    fillRect(r, {rc.x, rc.y, rc.w, t}, c);
    fillRect(r, {rc.x, rc.y + rc.h - t, rc.w, t}, c);
    fillRect(r, {rc.x, rc.y, t, rc.h}, c);
    fillRect(r, {rc.x + rc.w - t, rc.y, t, rc.h}, c);
}

static void fan(SDL_Renderer* r, float cx, float cy, const std::vector<SDL_FPoint>& rim, SDL_Color c) {
    if (rim.size() < 3) return;
    SDL_FColor fc = f(c);
    std::vector<SDL_Vertex> v;
    v.push_back({{cx, cy}, fc, {0, 0}});
    for (auto p : rim) v.push_back({p, fc, {0, 0}});
    std::vector<int> idx;
    int n = static_cast<int>(rim.size());
    for (int i = 0; i < n; ++i) {
        idx.push_back(0);
        idx.push_back(1 + i);
        idx.push_back(1 + (i + 1) % n);
    }
    SDL_RenderGeometry(r, nullptr, v.data(), static_cast<int>(v.size()), idx.data(), static_cast<int>(idx.size()));
}

void fillRound(SDL_Renderer* r, const SDL_FRect& rc, float rad, SDL_Color c) {
    rad = std::max(0.f, std::min({rad, rc.w * 0.5f, rc.h * 0.5f}));
    if (rad < 1.f) return fillRect(r, rc, c);
    const float PI = 3.14159265f;
    const SDL_FPoint centres[4] = {{rc.x + rc.w - rad, rc.y + rad},
                                   {rc.x + rc.w - rad, rc.y + rc.h - rad},
                                   {rc.x + rad, rc.y + rc.h - rad},
                                   {rc.x + rad, rc.y + rad}};
    std::vector<SDL_FPoint> rim;
    for (int k = 0; k < 4; ++k)
        for (int i = 0; i <= 6; ++i) {
            float a = -PI / 2 + k * PI / 2 + (PI / 2) * i / 6;
            rim.push_back({centres[k].x + std::cos(a) * rad, centres[k].y + std::sin(a) * rad});
        }
    fan(r, rc.x + rc.w / 2, rc.y + rc.h / 2, rim, c);
}

void fillCircle(SDL_Renderer* r, float cx, float cy, float radius, SDL_Color c) {
    std::vector<SDL_FPoint> rim;
    for (int i = 0; i < 24; ++i) {
        float a = 6.2831853f * i / 24;
        rim.push_back({cx + std::cos(a) * radius, cy + std::sin(a) * radius});
    }
    fan(r, cx, cy, rim, c);
}

void line(SDL_Renderer* r, float x1, float y1, float x2, float y2, float t, SDL_Color c) {
    float dx = x2 - x1, dy = y2 - y1, len = std::sqrt(dx * dx + dy * dy);
    if (len < 0.01f) return;
    float nx = -dy / len * t * 0.5f, ny = dx / len * t * 0.5f;
    SDL_FColor fc = f(c);
    SDL_Vertex v[4] = {{{x1 + nx, y1 + ny}, fc, {0, 0}},
                       {{x1 - nx, y1 - ny}, fc, {0, 0}},
                       {{x2 + nx, y2 + ny}, fc, {0, 0}},
                       {{x2 - nx, y2 - ny}, fc, {0, 0}}};
    int idx[6] = {0, 1, 2, 2, 1, 3};
    SDL_RenderGeometry(r, nullptr, v, 4, idx, 6);
}

void triangle(SDL_Renderer* r, SDL_FPoint a, SDL_FPoint b, SDL_FPoint c, SDL_Color col) {
    SDL_FColor fc = f(col);
    SDL_Vertex v[3] = {{a, fc, {0, 0}}, {b, fc, {0, 0}}, {c, fc, {0, 0}}};
    SDL_RenderGeometry(r, nullptr, v, 3, nullptr, 0);
}

float textWidth(const std::string& s, float scale) { return static_cast<float>(s.size()) * 8.f * scale; }

void text(SDL_Renderer* r, float x, float y, const std::string& s, float scale, SDL_Color c, Align a) {
    float w = textWidth(s, scale);
    if (a == Align::Center) x -= w / 2;
    else if (a == Align::Right) x -= w;
    SDL_SetRenderDrawColor(r, c.r, c.g, c.b, c.a);
    // The clip rect is in scaled coordinates, so scale it down to match the text.
    SDL_Rect clip;
    bool clipped = SDL_RenderClipEnabled(r) && SDL_GetRenderClipRect(r, &clip);
    SDL_SetRenderScale(r, scale, scale);
    if (clipped) {
        SDL_Rect sc{static_cast<int>(std::floor(clip.x / scale)), static_cast<int>(std::floor(clip.y / scale)),
                    static_cast<int>(std::ceil(clip.w / scale)), static_cast<int>(std::ceil(clip.h / scale))};
        SDL_SetRenderClipRect(r, &sc);
    }
    SDL_RenderDebugText(r, std::round(x) / scale, std::round(y) / scale, s.c_str());
    SDL_SetRenderScale(r, 1.f, 1.f);
    if (clipped) SDL_SetRenderClipRect(r, &clip);
}

std::vector<std::string> wrap(const std::string& s, size_t maxChars) {
    std::vector<std::string> out;
    std::istringstream in(s);
    std::string word, line;
    while (in >> word) {
        while (word.size() > maxChars) { // very long words get split
            if (!line.empty()) {
                out.push_back(line);
                line.clear();
            }
            out.push_back(word.substr(0, maxChars));
            word = word.substr(maxChars);
        }
        if (!line.empty() && line.size() + 1 + word.size() > maxChars) {
            out.push_back(line);
            line.clear();
        }
        if (!line.empty()) line += ' ';
        line += word;
    }
    if (!line.empty()) out.push_back(line);
    return out;
}

std::string fit(const std::string& s, float scale, float maxWidth) {
    if (textWidth(s, scale) <= maxWidth) return s;
    size_t n = static_cast<size_t>(std::max(0.f, maxWidth / (8.f * scale) - 3));
    return s.substr(0, n) + "...";
}

bool inside(float x, float y, const SDL_FRect& rc) { return x >= rc.x && y >= rc.y && x < rc.x + rc.w && y < rc.y + rc.h; }

SDL_Color alpha(SDL_Color c, Uint8 a) {
    c.a = a;
    return c;
}

std::string strf(const char* fmt, ...) {
    char buf[1024];
    va_list args;
    va_start(args, fmt);
    std::vsnprintf(buf, sizeof(buf), fmt, args);
    va_end(args);
    return buf;
}

std::string number(double v) {
    static const char* suffix[] = {"", "K", "M", "B", "T", "Qa", "Qi"};
    if (v < 0) return "-" + number(-v);
    if (v < 1000) return strf(v == std::floor(v) ? "%.0f" : "%.1f", v);
    int t = 0;
    while (v >= 1000 && t < 6) {
        v /= 1000;
        ++t;
    }
    return strf(v >= 100 ? "%.0f%s" : (v >= 10 ? "%.1f%s" : "%.2f%s"), v, suffix[t]);
}

SDL_Texture* loadImage(SDL_Renderer* r, const std::string& path) {
    size_t size = 0;
    void* data = SDL_LoadFile(path.c_str(), &size);
    if (!data) return nullptr;
    int w = 0, h = 0, n = 0;
    stbi_uc* px = stbi_load_from_memory(static_cast<const stbi_uc*>(data), static_cast<int>(size), &w, &h, &n, 4);
    SDL_free(data);
    if (!px) return nullptr;
    SDL_Texture* tex = nullptr;
    if (SDL_Surface* s = SDL_CreateSurfaceFrom(w, h, SDL_PIXELFORMAT_RGBA32, px, w * 4)) {
        tex = SDL_CreateTextureFromSurface(r, s);
        SDL_DestroySurface(s);
    }
    stbi_image_free(px);
    if (tex) SDL_SetTextureScaleMode(tex, SDL_SCALEMODE_LINEAR);
    return tex;
}

} // namespace gfx

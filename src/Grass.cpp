#include "Grass.h"

#include "Art.h"
#include "Draw.h"

#include <algorithm>
#include <cmath>
#include <vector>

namespace grass {

namespace {

constexpr int kTufts = 160;      // over the whole 1280 x 720 screen
constexpr int kRows = 6;         // mesh rows per tuft: more = smoother bend
constexpr float kTuftPx = 13.f;  // a tuft at size 1, in game pixels (the 16 x 16 sprite has a little room round it)

struct Tuft {
    float x, y;    // middle of its base
    float size;    // 0.6 .. 1.4
    float phase;   // so neighbours don't move in lockstep
    bool flip;
};

// The same field every time (it used to be painted into the background like this).
const std::vector<Tuft>& field() {
    static const std::vector<Tuft> tufts = [] {
        std::vector<Tuft> t;
        Uint32 seed = 12345u;
        auto rnd = [&seed] {
            seed = seed * 1664525u + 1013904223u;
            return static_cast<float>(seed >> 8) / static_cast<float>(1u << 24);
        };
        for (int i = 0; i < kTufts; ++i) {
            Tuft f;
            f.x = rnd() * 1280.f;
            f.y = 70.f + rnd() * 650.f;
            f.size = 0.6f + rnd() * 0.8f;
            f.phase = rnd() * 6.2832f;
            f.flip = rnd() < 0.5f;
            t.push_back(f);
        }
        // Back to front, so lower tufts overlap the ones above them.
        std::sort(t.begin(), t.end(), [](const Tuft& a, const Tuft& b) { return a.y < b.y; });
        return t;
    }();
    return tufts;
}

// How far the top of a tuft leans, as a fraction of its height (+ = right).
float lean(const Tuft& f, float t, float mx, float my) {
    float l = 0.10f * std::sin(t * 1.9f + f.phase)                    // its own gentle sway
              + 0.14f * std::sin(t * 0.7f - f.x * 0.006f + f.y * 0.002f) // a gust rolling across the field
              + 0.06f;                                               // the breeze blows a little to the right
    const float dx = f.x - mx, dy = (f.y - kTuftPx * f.size * 0.5f) - my;
    const float d = std::sqrt(dx * dx + dy * dy), reach = 70.f;
    if (d < reach) l += (dx >= 0.f ? 1.f : -1.f) * 0.55f * (1.f - d / reach); // brushed aside by the pointer
    return l;
}

} // namespace

void drawTuftBuiltin(SDL_Renderer* r, const SDL_FRect& rc) {
    const float s = rc.w / kTuftPx, x = rc.x + rc.w * 0.5f, y = rc.y + rc.h;
    const SDL_Color c{70, 128, 56, 255};
    draw::fillTriangle(r, {x - 5 * s, y}, {x - 1 * s, y}, {x - 6 * s, y - 10 * s}, c);
    draw::fillTriangle(r, {x - 2 * s, y}, {x + 2 * s, y}, {x, y - 13 * s}, c);
    draw::fillTriangle(r, {x + 1 * s, y}, {x + 5 * s, y}, {x + 6 * s, y - 9 * s}, c);
}

void draw(SDL_Renderer* r, float seconds, float mouseX, float mouseY) {
    SDL_Texture* tex = art::texture("farm/grass_tuft");
    if (!tex) { // no image: still tufts, just not moving
        for (const Tuft& f : field()) {
            const float w = kTuftPx * f.size;
            drawTuftBuiltin(r, SDL_FRect{f.x - w * 0.5f, f.y - w, w, w});
        }
        return;
    }
    float tw = 0, th = 0;
    SDL_GetTextureSize(tex, &tw, &th);
    // One mesh for the whole field: each tuft is a strip of kRows quads, its rows shifted
    // sideways by lean * (height above the base)^2, so the base stays put and the tips swing.
    static std::vector<SDL_Vertex> verts;
    static std::vector<int> idx;
    verts.clear();
    idx.clear();
    const SDL_FColor white{1.f, 1.f, 1.f, 1.f};
    for (const Tuft& f : field()) {
        const float w = kTuftPx * f.size * tw / 16.f, h = kTuftPx * f.size * th / 16.f;
        const float l = lean(f, seconds, mouseX, mouseY) * h;
        const int base = static_cast<int>(verts.size());
        for (int k = 0; k <= kRows; ++k) {
            const float v = static_cast<float>(k) / kRows;  // 0 = top, 1 = base
            const float up = 1.f - v;
            const float y = f.y - h * up;
            const float x = f.x + l * up * up;
            const float u0 = f.flip ? 1.f : 0.f, u1 = f.flip ? 0.f : 1.f;
            verts.push_back(SDL_Vertex{{x - w * 0.5f, y}, white, {u0, v}});
            verts.push_back(SDL_Vertex{{x + w * 0.5f, y}, white, {u1, v}});
        }
        for (int k = 0; k < kRows; ++k) {
            const int a = base + k * 2;
            idx.insert(idx.end(), {a, a + 1, a + 2, a + 1, a + 3, a + 2});
        }
    }
    SDL_RenderGeometry(r, tex, verts.data(), static_cast<int>(verts.size()), idx.data(), static_cast<int>(idx.size()));
}

void drawField(SDL_Renderer* r, Uint8 darken) {
    art::draw(r, "farm/background", SDL_FRect{0, 0, 1280, 720});
    float wx = -1000.f, wy = -1000.f, mx = -1000.f, my = -1000.f;
    SDL_Window* win = SDL_GetRenderWindow(r);
    if (win && SDL_GetMouseFocus() == win && SDL_GetRenderTarget(r) == nullptr) {
        SDL_GetMouseState(&wx, &wy);
        SDL_RenderCoordinatesFromWindow(r, wx, wy, &mx, &my); // into the game's 1280 x 720
    }
    draw(r, static_cast<float>(SDL_GetTicks()) / 1000.f, mx, my);
    if (darken > 0) draw::fillRect(r, 0, 0, 1280, 720, SDL_Color{0, 0, 0, darken});
}

} // namespace grass

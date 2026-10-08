// Palette.h - IncraVegetable's muted palette (32 colours, in ramps per
// material: low saturation, warm bias). The same colours as the GIMP palette
// art_templates/IncraVegetable.gpl (and art_templates/palette.png), for painting art that matches.
//
// All text is drawn in these colours: draw::text() snaps any other colour to
// the nearest one here, so even a stray SDL_Color{255,255,255} comes out as
// Cream. Pick the colour you mean from this list instead of relying on that.
#pragma once

#include <SDL3/SDL.h>

#include <array>
#include <cmath>

namespace pal {

// Shadow & UI
constexpr SDL_Color NightSoil{31, 36, 33, 255};
constexpr SDL_Color Panel{46, 53, 48, 255};
constexpr SDL_Color Slate{72, 81, 74, 255};
constexpr SDL_Color Stone{122, 130, 121, 255};
constexpr SDL_Color SageMist{180, 182, 166, 255};
constexpr SDL_Color Parchment{230, 224, 204, 255};
constexpr SDL_Color Cream{244, 239, 225, 255};
// Soil & wood
constexpr SDL_Color DeepSoil{59, 42, 34, 255};
constexpr SDL_Color Soil{90, 64, 49, 255};
constexpr SDL_Color Tilled{122, 90, 66, 255};
constexpr SDL_Color Timber{156, 122, 88, 255};
constexpr SDL_Color Straw{194, 162, 122, 255};
// Leaves & grass
constexpr SDL_Color Moss{47, 66, 48, 255};
constexpr SDL_Color LeafShade{71, 97, 63, 255};
constexpr SDL_Color Leaf{98, 127, 80, 255};
constexpr SDL_Color FreshLeaf{132, 160, 101, 255};
constexpr SDL_Color PaleShoot{178, 194, 140, 255};
// Carrot & pumpkin
constexpr SDL_Color Burnt{138, 75, 44, 255};
constexpr SDL_Color Carrot{184, 102, 58, 255};
constexpr SDL_Color Pumpkin{212, 140, 85, 255};
constexpr SDL_Color Skin{220, 176, 140, 255};
// Coin & gold
constexpr SDL_Color Mustard{168, 138, 60, 255};
constexpr SDL_Color Coin{212, 178, 94, 255};
constexpr SDL_Color PaleGold{236, 215, 154, 255};
// Brick
constexpr SDL_Color Brick{125, 58, 53, 255};
constexpr SDL_Color Rosehip{168, 82, 74, 255};
// Sky & denim
constexpr SDL_Color DeepDenim{52, 74, 92, 255};
constexpr SDL_Color Denim{80, 112, 138, 255};
constexpr SDL_Color Sky{127, 155, 176, 255};
constexpr SDL_Color Mist{185, 202, 212, 255};
// Dusk
constexpr SDL_Color Dusk{90, 74, 102, 255};
constexpr SDL_Color Lavender{138, 122, 152, 255};

constexpr std::array<SDL_Color, 32> kAll = {
    NightSoil, Panel,  Slate,     Stone, SageMist, Parchment, Cream,   DeepSoil, Soil,   Tilled,  Timber,
    Straw,     Moss,   LeafShade, Leaf,  FreshLeaf, PaleShoot, Burnt,  Carrot,   Pumpkin, Skin,   Mustard,
    Coin,      PaleGold, Brick,   Rosehip, DeepDenim, Denim,   Sky,     Mist,     Dusk,   Lavender};

// The palette colour nearest to `c` (by eye, "redmean" distance), keeping c's alpha.
inline SDL_Color nearest(SDL_Color c) {
    SDL_Color best = kAll[0];
    float bestD = 1e30f;
    for (const SDL_Color& p : kAll) {
        float rm = (c.r + p.r) * 0.5f;
        float dr = static_cast<float>(c.r) - p.r, dg = static_cast<float>(c.g) - p.g, db = static_cast<float>(c.b) - p.b;
        float d = (2.f + rm / 256.f) * dr * dr + 4.f * dg * dg + (2.f + (255.f - rm) / 256.f) * db * db;
        if (d < bestD) {
            bestD = d;
            best = p;
        }
    }
    best.a = c.a;
    return best;
}

// A palette colour with a different alpha.
constexpr SDL_Color alpha(SDL_Color c, Uint8 a) { return SDL_Color{c.r, c.g, c.b, a}; }

} // namespace pal

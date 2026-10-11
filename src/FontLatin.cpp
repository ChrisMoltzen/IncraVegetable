// FontLatin.cpp - the accented letters (é, ñ, ü, ç, ß ...) for other languages.
//
// The font image (assets/ui/font.png) has the 95 plain characters. The
// characters U+00A0..U+00FF (Latin-1: enough for Spanish, French, German,
// Portuguese, Italian, Dutch, the Nordic languages...) come from a second
// image, assets/ui/font_latin1.png, laid out the same way: 16 across and 6 down,
// in order from U+00A0. Without that image the game makes them itself, from
// font.png: the plain letter with the accent drawn over it.
#include "FontLatin.h"

#include "Art.h"

#include <algorithm>
#include <cmath>
#include <cstring>
#include <vector>

namespace fontlatin {

namespace {

enum Mark { None, Acute, Grave, Circumflex, Diaeresis, Tilde, Ring, Cedilla, Slash, Macron, Flip, Raise };

struct Recipe {
    char base;
    Mark mark;
};

// U+00A0 .. U+00FF.
const Recipe kRecipes[96] = {
    {' ', None},      {'!', Flip},      {'c', None},      {'L', None},       // nbsp ¡ ¢ £
    {'o', None},      {'Y', None},      {'|', None},      {'S', None},       // ¤ ¥ ¦ §
    {' ', Diaeresis}, {'C', None},      {'a', Raise},     {'<', None},       // ¨ © ª «
    {'-', None},      {'-', None},      {'R', None},      {' ', Macron},     // ¬ soft-hyphen ® ¯
    {'o', Raise},     {'+', None},      {'2', Raise},     {'3', Raise},      // ° ± ² ³
    {' ', Acute},     {'u', None},      {'P', None},      {'.', Raise},      // ´ µ ¶ ·
    {' ', Cedilla},   {'1', Raise},     {'o', Raise},     {'>', None},       // ¸ ¹ º »
    {'?', None},      {'?', None},      {'?', None},      {'?', Flip},       // ¼ ½ ¾ ¿
    {'A', Grave},     {'A', Acute},     {'A', Circumflex}, {'A', Tilde},     // À Á Â Ã
    {'A', Diaeresis}, {'A', Ring},      {'E', None},      {'C', Cedilla},    // Ä Å Æ Ç
    {'E', Grave},     {'E', Acute},     {'E', Circumflex}, {'E', Diaeresis}, // È É Ê Ë
    {'I', Grave},     {'I', Acute},     {'I', Circumflex}, {'I', Diaeresis}, // Ì Í Î Ï
    {'D', None},      {'N', Tilde},     {'O', Grave},     {'O', Acute},      // Ð Ñ Ò Ó
    {'O', Circumflex}, {'O', Tilde},    {'O', Diaeresis}, {'x', None},       // Ô Õ Ö ×
    {'O', Slash},     {'U', Grave},     {'U', Acute},     {'U', Circumflex}, // Ø Ù Ú Û
    {'U', Diaeresis}, {'Y', Acute},     {'P', None},      {'B', None},       // Ü Ý Þ ß (drawn below)
    {'a', Grave},     {'a', Acute},     {'a', Circumflex}, {'a', Tilde},     // à á â ã
    {'a', Diaeresis}, {'a', Ring},      {'e', None},      {'c', Cedilla},    // ä å æ ç
    {'e', Grave},     {'e', Acute},     {'e', Circumflex}, {'e', Diaeresis}, // è é ê ë
    {'i', Grave},     {'i', Acute},     {'i', Circumflex}, {'i', Diaeresis}, // ì í î ï
    {'d', None},      {'n', Tilde},     {'o', Grave},     {'o', Acute},      // ð ñ ò ó
    {'o', Circumflex}, {'o', Tilde},    {'o', Diaeresis}, {'/', None},       // ô õ ö ÷
    {'o', Slash},     {'u', Grave},     {'u', Acute},     {'u', Circumflex}, // ø ù ú û
    {'u', Diaeresis}, {'y', Acute},     {'p', None},      {'y', Diaeresis},  // ü ý þ ÿ
};

// Accent shapes on an 8 x 8 grid of "font pixels" (the font is drawn at 2x, so
// one of these is 2 x 2 image pixels). Columns are relative to the letter's
// middle. Small letters get two rows above them; capitals are moved down one
// row and get a one-row accent.
struct Dot {
    int row, col;
};
std::vector<Dot> markDots(Mark m, bool capital) {
    if (capital) {
        switch (m) {
        case Acute: return {{0, 0}, {0, 1}};
        case Grave: return {{0, -1}, {0, 0}};
        case Circumflex: return {{0, -1}, {0, 1}, {0, 0}};
        case Diaeresis: return {{0, -1}, {0, 1}};
        case Tilde: return {{0, -2}, {0, -1}, {0, 1}};
        case Ring: return {{0, 0}};
        default: break;
        }
    }
    switch (m) {
    case Acute: return {{0, 1}, {1, 0}};
    case Grave: return {{0, -1}, {1, 0}};
    case Circumflex: return {{0, 0}, {1, -1}, {1, 1}};
    case Diaeresis: return {{1, -1}, {1, 1}};
    case Tilde: return {{0, -1}, {0, 1}, {1, -2}, {1, 0}};
    case Ring: return {{0, -1}, {0, 0}, {1, -1}, {1, 0}};
    case Macron: return {{1, -1}, {1, 0}, {1, 1}};
    default: return {};
    }
}

Uint32 pixel(SDL_Surface* s, int x, int y) {
    if (x < 0 || y < 0 || x >= s->w || y >= s->h) return 0;
    return static_cast<Uint32*>(s->pixels)[y * (s->pitch / 4) + x];
}
void setPixel(SDL_Surface* s, int x, int y, Uint32 v) {
    if (x < 0 || y < 0 || x >= s->w || y >= s->h) return;
    static_cast<Uint32*>(s->pixels)[y * (s->pitch / 4) + x] = v;
}
bool solid(Uint32 p) { return (p >> 24) > 0; } // ABGR8888 / RGBA32: alpha is the top byte

// Builds the 16 x 6 Latin-1 sheet from the plain font.
SDL_Surface* build(SDL_Surface* font) {
    const int cw = font->w / 16, ch = font->h / 6, u = std::max(1, cw / 8);
    SDL_Surface* out = SDL_CreateSurface(font->w, font->h, SDL_PIXELFORMAT_RGBA32);
    if (!out) return nullptr;
    SDL_FillSurfaceRect(out, nullptr, 0);
    Uint32 ink = 0xFFFFFFFFu;
    for (int i = 0; i < 96; ++i) {
        const Recipe& rc = kRecipes[i];
        const int src = static_cast<unsigned char>(rc.base) - 32;
        const int sx = (src % 16) * cw, sy = (src / 16) * ch;
        const int dx = (i % 16) * cw, dy = (i / 16) * ch;
        // The letter's columns, and its colour (the font may not be white).
        int minX = cw, maxX = -1, minY = ch;
        for (int y = 0; y < ch; ++y)
            for (int x = 0; x < cw; ++x)
                if (solid(pixel(font, sx + x, sy + y))) {
                    minX = std::min(minX, x);
                    maxX = std::max(maxX, x);
                    minY = std::min(minY, y);
                    ink = pixel(font, sx + x, sy + y);
                }
        const bool capital = (rc.base >= 'A' && rc.base <= 'Z');
        const bool dotless = rc.mark != None && (rc.base == 'i' || rc.base == 'j');
        const int shift = capital && rc.mark != None && rc.mark != Cedilla && rc.mark != Slash ? u : 0;
        const int raise = rc.mark == Raise && minY < ch ? minY : 0; // up to the top (° · ª º)
        for (int y = 0; y < ch; ++y)
            for (int x = 0; x < cw; ++x) {
                int fx = x, fy = y;
                if (rc.mark == Flip) fx = cw - 1 - x, fy = ch - 1 - y; // upside down (¡ ¿)
                if (dotless && fy < 2 * u) continue;
                Uint32 p = pixel(font, sx + fx, sy + fy);
                if (solid(p)) setPixel(out, dx + x, dy + y + shift - raise, p);
            }
        if (i == 0xDF - 0xA0) { // ß: drawn from scratch, on the same 8 x 8 grid
            SDL_Rect clear{dx, dy, cw, ch};
            SDL_FillSurfaceRect(out, &clear, 0);
            const char* rows[8] = {".XXX....", "X...X...", "X..X....", "X...X...", "X....X..",
                                   "X....X..", "X.XXX...", "........"};
            for (int y = 0; y < 8; ++y)
                for (int x = 0; x < 8; ++x)
                    if (rows[y][x] == 'X') {
                        SDL_Rect b{dx + x * u, dy + y * u, u, u};
                        SDL_FillSurfaceRect(out, &b, ink);
                    }
            continue;
        }
        if (maxX < 0) maxX = cw / 2, minX = cw / 2 - 1; // an accent on its own (´ ¨ ¸)
        const int mid = (minX + maxX + 1) / 2;
        auto dot = [&](int row, int col) { // one font pixel, snapped to the font's grid
            const int left = static_cast<int>(std::lround((mid + col * u - u / 2.0) / u)) * u;
            SDL_Rect b{dx + left, dy + row * u, u, u};
            SDL_FillSurfaceRect(out, &b, ink);
        };
        if (rc.mark == Cedilla) {
            dot(7, 0);
            dot(7, -1);
        } else if (rc.mark == Slash) {
            const int top = capital ? 0 : 2;
            for (int k = 0; top + k <= 6; ++k) dot(6 - k, -2 + (k * 4) / std::max(1, 6 - top));
        } else {
            for (const Dot& d : markDots(rc.mark, capital)) dot(d.row, d.col);
        }
    }
    return out;
}

struct Cache {
    SDL_Renderer* renderer = nullptr;
    SDL_Texture* texture = nullptr;
    bool tried = false;
} gCache;

} // namespace

SDL_Texture* texture(SDL_Renderer* r) {
    if (SDL_Texture* own = art::texture("ui/font_latin1")) return own;
    if (gCache.renderer != r) {
        gCache = Cache{};
        gCache.renderer = r;
    }
    if (gCache.tried) return gCache.texture;
    gCache.tried = true;
    SDL_Surface* font = art::loadSurface("ui/font");
    if (!font) return nullptr;
    SDL_Surface* rgba = SDL_ConvertSurface(font, SDL_PIXELFORMAT_RGBA32);
    SDL_DestroySurface(font);
    if (!rgba) return nullptr;
    SDL_Surface* sheet = build(rgba);
    SDL_DestroySurface(rgba);
    if (!sheet) return nullptr;
    gCache.texture = SDL_CreateTextureFromSurface(r, sheet);
    SDL_DestroySurface(sheet);
    if (gCache.texture) SDL_SetTextureScaleMode(gCache.texture, SDL_SCALEMODE_NEAREST);
    return gCache.texture;
}

void forget() {
    if (gCache.texture) SDL_DestroyTexture(gCache.texture);
    gCache = Cache{};
}

char plain(unsigned cp) {
    if (cp >= 0xA0 && cp <= 0xFF) {
        if (cp == 0xDF) return 's';
        return kRecipes[cp - 0xA0].base;
    }
    return '?';
}

void drawTemplate(SDL_Renderer* r, const SDL_FRect& rc) {
    if (SDL_Texture* t = texture(r)) SDL_RenderTexture(r, t, nullptr, &rc);
}

} // namespace fontlatin

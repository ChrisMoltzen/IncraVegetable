#include "Screen.h"

#include "Art.h"
#include "Draw.h"

#include <cmath>

namespace screen {

namespace {
// The canvas (whole screen) size in game units, and where the stage sits in it.
int gW = kStageW, gH = kStageH;
int gX = 0, gY = 0;

bool drawingToScreen(SDL_Renderer* r) { return kFillScreen && SDL_GetRenderTarget(r) == nullptr; }

void useStage(SDL_Renderer* r) {
    SDL_Rect vp{gX, gY, kStageW, kStageH};
    SDL_SetRenderViewport(r, &vp);
}
} // namespace

void fit(SDL_Renderer* r) {
    if (!drawingToScreen(r)) return;
    int pw = 0, ph = 0;
    if (SDL_GetRenderOutputSize(r, &pw, &ph) && pw > 0 && ph > 0) {
        // Grow whichever side is short of the screen's shape, so the stage
        // always fits whole and the canvas matches the screen exactly.
        const double aspect = static_cast<double>(pw) / ph;
        const double stage = static_cast<double>(kStageW) / kStageH;
        int w = kStageW, h = kStageH;
        if (aspect > stage) w = static_cast<int>(std::lround(kStageH * aspect));
        else h = static_cast<int>(std::lround(kStageW / aspect));
        if (w != gW || h != gH) {
            gW = w;
            gH = h;
            gX = (gW - kStageW) / 2;
            gY = (gH - kStageH) / 2;
            SDL_SetRenderLogicalPresentation(r, gW, gH, SDL_LOGICAL_PRESENTATION_LETTERBOX);
        }
    }
    useStage(r);
}

SDL_FRect bounds() {
    if (!kFillScreen) return SDL_FRect{0, 0, kStageW, kStageH};
    return SDL_FRect{static_cast<float>(-gX), static_cast<float>(-gY), static_cast<float>(gW), static_cast<float>(gH)};
}

Whole::Whole(SDL_Renderer* r) : r_(r) {
    if (!drawingToScreen(r) || (gW == kStageW && gH == kStageH)) return;
    active_ = true;
    SDL_SetRenderViewport(r, nullptr);
    ox = static_cast<float>(gX);
    oy = static_cast<float>(gY);
    w = static_cast<float>(gW);
    h = static_cast<float>(gH);
}

Whole::~Whole() {
    if (active_) useStage(r_);
}

void fillAll(SDL_Renderer* r, SDL_Color c) {
    Whole all(r);
    draw::fillRect(r, 0, 0, all.w, all.h, c);
}

void background(SDL_Renderer* r, const std::string& name) {
    Whole all(r);
    const SDL_FRect st = all.stage(SDL_FRect{0, 0, kStageW, kStageH});
    SDL_Texture* tex = art::texture(name);
    if (!tex) { // a built-in drawing: just stretch it over everything
        art::draw(r, name, SDL_FRect{0, 0, all.w, all.h});
        return;
    }
    SDL_RenderTexture(r, tex, nullptr, &st);
    if (all.ox <= 0.f && all.oy <= 0.f) return;
    float tw = 0, th = 0;
    SDL_GetTextureSize(tex, &tw, &th);
    // The outermost row or column of pixels, stretched over each margin.
    auto edge = [&](SDL_FRect src, SDL_FRect dst) {
        if (dst.w > 0.f && dst.h > 0.f) SDL_RenderTexture(r, tex, &src, &dst);
    };
    const float right = st.x + st.w, bottom = st.y + st.h;
    edge({0, 0, 1, th}, {0, st.y, st.x, st.h});                          // left
    edge({tw - 1, 0, 1, th}, {right, st.y, all.w - right, st.h});        // right
    edge({0, 0, tw, 1}, {st.x, 0, st.w, st.y});                          // top
    edge({0, th - 1, tw, 1}, {st.x, bottom, st.w, all.h - bottom});      // bottom
    edge({0, 0, 1, 1}, {0, 0, st.x, st.y});                              // corners
    edge({tw - 1, 0, 1, 1}, {right, 0, all.w - right, st.y});
    edge({0, th - 1, 1, 1}, {0, bottom, st.x, all.h - bottom});
    edge({tw - 1, th - 1, 1, 1}, {right, bottom, all.w - right, all.h - bottom});
}

void topBar(SDL_Renderer* r, const std::string& name, float height) {
    Whole all(r);
    const SDL_FRect bar{0, all.oy, all.w, height};
    art::draw(r, name, bar);
    if (all.oy <= 0.f) return;
    // Above the stage (taller screens): a row from the middle of the bar, stretched up to the top edge.
    if (SDL_Texture* tex = art::texture(name)) {
        float tw = 0, th = 0;
        SDL_GetTextureSize(tex, &tw, &th);
        const SDL_FRect src{0, std::floor(th * 0.5f), tw, 1};
        const SDL_FRect dst{0, 0, all.w, all.oy};
        SDL_RenderTexture(r, tex, &src, &dst);
    } else {
        art::draw(r, name, SDL_FRect{0, 0, all.w, all.oy + height * 0.5f});
        art::draw(r, name, bar);
    }
}

} // namespace screen

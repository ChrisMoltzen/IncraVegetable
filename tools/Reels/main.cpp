// Reels - renders vertical (1080x1920, 30 fps) short videos of the game for
// Instagram Reels / TikTok / YouTube Shorts, straight from the game code:
//   01_day1_to_day140.mp4  the patch growing from Day 1 to Day 140
//   02_every_upgrade.mp4   a late-game day with every upgrade bought
//   03_the_barn.mp4        the tech tree being bought up, snowflake and all
// Each ends on a title card. Sound is the game's own music and effects.
//
// Build and run with `make reels` (needs ffmpeg on your PATH); the videos land
// in build/reels/. The end card's line can be changed:  ./Reels "Out now on itch.io"
//
// It plays the game by itself, using a save folder of its own (build/reels/saves/),
// so your real saves are never touched. It reaches into the game's private parts
// to drive it, hence the #define below; it's a tool, not part of the game.
#include <sstream>
#include <map>
#include <random>
#include <vector>
#include <string>
#include <functional>
#include <cstdio>
#include <cmath>
#include <algorithm>
#define private public
#include "Game.h"
#undef private
#include "Art.h"
#include "Draw.h"
#include "Palette.h"
#include "UI.h"

static const int W = 1080, H = 1920, FPS = 30;
static std::string kEndLine = "Play it on itch.io";

struct Caption { std::string text; float scale; SDL_Color color; float y; int startFrame; };

struct Reel {
    Game* g = nullptr;
    SDL_Texture* gameTex = nullptr;
    SDL_Texture* vertTex = nullptr;
    std::FILE* pipe = nullptr;
    std::vector<float> audio;
    std::string name;
    int frame = 0;
    SDL_FRect crop{370, 0, 540, 720}; // part of the 1280x720 game shown, scaled 2x
    float cropY = 260;                // where it goes on the 1080x1920 frame
    std::vector<Caption> top, bottom;
    bool showCursor = true;
    bool coinCounter = true;
    std::string counterText; // replaces the coin counter if set
    float fade = 0.f;        // 0..1 cover in Night Soil
};

static void cursor(SDL_Renderer* r, float x, float y, float s) {
    static const char* C[] = {"X...........", "XX..........", "XWX.........", "XWWX........", "XWWWX.......",
                              "XWWWWX......", "XWWWWWX.....", "XWWWWWWX....", "XWWWWWWWX...", "XWWWWWWWWX..",
                              "XWWWWWWWWWX.", "XWWWWWWXXXXX", "XWWWXWWX....", "XWWX.XWWX...", "XWX..XWWX...",
                              "XX....XWWX..", "X.....XWWX..", ".......XX..."};
    for (int j = 0; j < 18; ++j)
        for (int i = 0; i < 12; ++i) {
            char c = C[j][i];
            if (c == '.') continue;
            draw::fillRect(r, x + i * s, y + j * s, s, s, c == 'X' ? pal::NightSoil : pal::Cream);
        }
}

static void begin(Reel& v, Game& g, const std::string& name) {
    v.g = &g; v.name = name; v.frame = 0; v.audio.clear(); v.top.clear(); v.bottom.clear();
    SDL_Renderer* r = g.renderer();
    if (!v.gameTex) {
        v.gameTex = SDL_CreateTexture(r, SDL_PIXELFORMAT_RGBA8888, SDL_TEXTUREACCESS_TARGET, 1280, 720);
        SDL_SetTextureScaleMode(v.gameTex, SDL_SCALEMODE_NEAREST);
        v.vertTex = SDL_CreateTexture(r, SDL_PIXELFORMAT_RGBA8888, SDL_TEXTUREACCESS_TARGET, W, H);
    }
    std::string cmd = "ffmpeg -loglevel error -y -f rawvideo -pix_fmt rgba -s 1080x1920 -r 30 -i - -c:v libx264 -preset slow -crf 18 -pix_fmt yuv420p " + name + "_video.mp4";
    v.pipe = popen(cmd.c_str(), "w");
}

static void drawCaptions(SDL_Renderer* r, Reel& v, const std::vector<Caption>& cs) {
    for (const Caption& c : cs) {
        int age = v.frame - c.startFrame;
        if (age < 0) continue;
        float t = std::min(1.f, age / 6.f);
        float ease = 1.f - (1.f - t) * (1.f - t);
        Uint8 a = static_cast<Uint8>(255 * ease);
        float y = c.y + (1.f - ease) * 30.f;
        draw::text(r, W * 0.5f + c.scale, y + c.scale, c.text, c.scale, pal::alpha(pal::NightSoil, a), draw::Align::Center);
        draw::text(r, W * 0.5f, y, c.text, c.scale, pal::alpha(c.color, a), draw::Align::Center);
    }
}

// One video frame: the game as it is now, framed for a phone.
static void frame(Reel& v, bool renderGame = true, std::function<void(SDL_Renderer*)> overGame = nullptr,
                  std::function<void(SDL_Renderer*)> custom = nullptr) {
    Game& g = *v.g;
    SDL_Renderer* r = g.renderer();
    if (renderGame) {
        SDL_SetRenderTarget(r, v.gameTex);
        g.render(false);
        if (overGame) overGame(r);
        if (v.showCursor && g.state_ == Game::State::Farming) cursor(r, g.mouseX_, g.mouseY_, 2.f);
    }
    SDL_SetRenderTarget(r, v.vertTex);
    if (custom) {
        custom(r);
    } else {
        draw::fillRect(r, 0, 0, W, H, pal::Panel);
        const float k = W / v.crop.w; // 2x for a 540-wide crop, 3x for 360
        SDL_FRect dst{0, v.cropY, W, v.crop.h * k};
        draw::fillRect(r, 0, dst.y - 6, W, dst.h + 12, pal::NightSoil);
        SDL_RenderTexture(r, v.gameTex, &v.crop, &dst);
        drawCaptions(r, v, v.top);
        drawCaptions(r, v, v.bottom);
        float by = dst.y + dst.h + 70;
        if (!v.counterText.empty()) {
            draw::text(r, W * 0.5f + 5, by + 5, v.counterText, 5.f, pal::NightSoil, draw::Align::Center);
            draw::text(r, W * 0.5f, by, v.counterText, 5.f, pal::PaleGold, draw::Align::Center);
        } else if (v.coinCounter) {
            std::string s = draw::number(g.coins_);
            float tw = draw::textWidth(s, 6.f), x0 = W * 0.5f - (tw + 70) * 0.5f;
            art::draw(r, "ui/coin", SDL_FRect{x0, by - 4, 56, 56});
            draw::text(r, x0 + 76 + 6, by + 6, s, 6.f, pal::NightSoil);
            draw::text(r, x0 + 76, by, s, 6.f, pal::Coin);
        }
    }
    if (v.fade > 0.f) draw::fillRect(r, 0, 0, W, H, pal::alpha(pal::NightSoil, static_cast<Uint8>(255 * std::min(1.f, v.fade))));
    SDL_Surface* s = SDL_RenderReadPixels(r, nullptr);
    SDL_Surface* rgba = SDL_ConvertSurface(s, SDL_PIXELFORMAT_ABGR8888); // bytes R,G,B,A
    for (int y = 0; y < H; ++y) std::fwrite(static_cast<Uint8*>(rgba->pixels) + y * rgba->pitch, 1, W * 4, v.pipe);
    SDL_DestroySurface(rgba);
    SDL_DestroySurface(s);
    SDL_SetRenderTarget(r, nullptr);
    // Audio for this frame, mixed offline.
    const int n = 44100 / FPS;
    std::vector<float> buf(n * 2);
    g.audio_.mix(buf.data(), n);
    v.audio.insert(v.audio.end(), buf.begin(), buf.end());
    ++v.frame;
}

static void writeWav(const std::string& fn, const std::vector<float>& a) {
    std::FILE* f = std::fopen(fn.c_str(), "wb");
    auto u32 = [&](uint32_t x) { std::fwrite(&x, 4, 1, f); };
    auto u16 = [&](uint16_t x) { std::fwrite(&x, 2, 1, f); };
    const uint32_t bytes = static_cast<uint32_t>(a.size() * 4);
    std::fwrite("RIFF", 1, 4, f); u32(36 + bytes); std::fwrite("WAVEfmt ", 1, 8, f);
    u32(16); u16(3); u16(2); u32(44100); u32(44100 * 8); u16(8); u16(32); // 32-bit float: no clipping here
    std::fwrite("data", 1, 4, f); u32(bytes);
    std::fwrite(a.data(), 4, a.size(), f);
    std::fclose(f);
}

static void end(Reel& v) {
    pclose(v.pipe);
    writeWav(v.name + "_audio.wav", v.audio);
    std::string cmd = "ffmpeg -loglevel error -y -i " + v.name + "_video.mp4 -i " + v.name +
                      "_audio.wav -c:v copy -af alimiter=limit=0.7:level=false,loudnorm=I=-14:TP=-1.5:LRA=11 -ar 44100 -c:a aac -b:a 192k -shortest -movflags +faststart " + v.name + ".mp4";
    if (std::system(cmd.c_str()) != 0) std::printf("ffmpeg couldn't add the sound to %s\n", v.name.c_str());
    else {
        std::remove((v.name + "_video.mp4").c_str());
        std::remove((v.name + "_audio.wav").c_str());
    }
    std::printf("%s: %d frames (%.1fs)\n", v.name.c_str(), v.frame, v.frame / float(FPS));
}

// --- Game helpers ---------------------------------------------------------
static void buyUpTo(Game& g, double budget) {
    auto& tree = g.tree_;
    for (;;) {
        TechNode* best = nullptr;
        for (auto& n : tree.nodes())
            if (n.level == 0 && tree.prereqsMet(n) && (!best || tree.cost(n) < tree.cost(*best))) best = &n;
        if (!best || tree.cost(*best) > budget) break;
        best->level = 1;
    }
}
// Moves the pointer like a player: glide to the nearest ripe crop inside the shown area and hold it there.
struct Picker {
    int target = -1; float dwell = 0.f;
    float minX = 420, maxX = 860, minY = 120, maxY = 690;
    void step(Game& g) {
        auto& tiles = g.farm_.tiles_;
        bool bad = target < 0 || target >= (int)tiles.size() || tiles[target].growth < 1.f || dwell > 2.5f;
        if (bad) {
            float best = 1e9f; target = -1;
            for (int k = 0; k < (int)tiles.size(); ++k) {
                const auto& t = tiles[k];
                if (t.growth < 1.f || t.x < minX || t.x > maxX || t.y < minY || t.y > maxY) continue;
                float dx = t.x - g.mouseX_, dy = t.y - g.mouseY_, d = dx * dx + dy * dy;
                if (d < best) { best = d; target = k; }
            }
            dwell = 0.f;
        }
        if (target >= 0) {
            g.mouseX_ += (tiles[target].x - g.mouseX_) * 0.3f;
            g.mouseY_ += (tiles[target].y - g.mouseY_) * 0.3f;
            dwell += 1.f / FPS;
        }
        g.mouseInside_ = true;
    }
};
static void farmFrames(Reel& v, Picker& p, int frames) {
    Game& g = *v.g;
    for (int i = 0; i < frames; ++i) {
        p.step(g);
        g.update(1.f / FPS);
        frame(v);
    }
}
static void warm(Game& g, Picker& p, float seconds) { // play without recording
    for (int i = 0; i < seconds * FPS; ++i) { p.step(g); g.update(1.f / FPS); }
}

static void endCard(Reel& v, int frames) {
    Game& g = *v.g;
    SDL_Renderer* r0 = g.renderer();
    SDL_Color sky{250, 168, 92, 255};
    { // the colour at the top of the menu's sky, to carry it up the tall frame
        SDL_SetRenderTarget(r0, v.gameTex);
        g.mainMenu_.renderBackground(r0);
        SDL_Rect px{640, 0, 1, 1};
        if (SDL_Surface* s = SDL_RenderReadPixels(r0, &px)) {
            SDL_ReadSurfacePixel(s, 0, 0, &sky.r, &sky.g, &sky.b, &sky.a);
            SDL_DestroySurface(s);
        }
        SDL_SetRenderTarget(r0, nullptr);
    }
    for (int i = 0; i < frames; ++i) {
        g.mainMenu_.update(1.f / FPS);
        float t = std::min(1.f, i / 10.f);
        frame(v, false, nullptr, [&](SDL_Renderer* r) {
            SDL_SetRenderTarget(r, v.gameTex);
            g.mainMenu_.renderBackground(r);
            SDL_SetRenderTarget(r, v.vertTex);
            SDL_SetRenderDrawColor(r, sky.r, sky.g, sky.b, 255);
            SDL_FRect top{0, 0, (float)W, 482};
            SDL_RenderFillRect(r, &top);
            SDL_FRect src{600, 0, 540, 720}, dst{0, 480, 1080, 1440}; // with the sun in
            SDL_RenderTexture(r, v.gameTex, &src, &dst);
            const float sc = 9.f, bounce = std::round(std::sin(i * 0.12f) * 6.f);
            const float tw = draw::textWidth("IncraVegetable", sc), x = (W - tw) * 0.5f, y = 560 + bounce;
            Uint8 a = static_cast<Uint8>(255 * t);
            draw::text(r, x + 9, y + 9, "IncraVegetable", sc, pal::alpha(pal::DeepSoil, a));
            draw::text(r, x, y, "Incra", sc, pal::alpha(pal::FreshLeaf, a));
            draw::text(r, x + draw::textWidth("Incra", sc), y, "Vegetable", sc, pal::alpha(pal::Pumpkin, a));
            draw::text(r, W * 0.5f, y + 110, "an incremental farming game", 3.f, pal::alpha(pal::Burnt, a), draw::Align::Center);
            float t2 = std::clamp((i - 12) / 10.f, 0.f, 1.f);
            Uint8 a2 = static_cast<Uint8>(255 * t2);
            draw::text(r, W * 0.5f + 5, 820 + 5, kEndLine.c_str(), 5.f, pal::alpha(pal::DeepSoil, a2), draw::Align::Center);
            draw::text(r, W * 0.5f, 820, kEndLine.c_str(), 5.f, pal::alpha(pal::Cream, a2), draw::Align::Center);
        });
    }
}

static void fadeFrames(Reel& v, int frames, bool out, std::function<void()> step) {
    for (int i = 0; i < frames; ++i) {
        float t = (i + 1) / float(frames);
        v.fade = out ? t : 1.f - t;
        if (step) step();
        frame(v);
    }
    v.fade = 0.f;
}

static std::string farmLine(Game& g) {
    Stats s = g.tree_.computeStats();
    int crops = 0;
    for (Crop c = 0; c < cropCount(); ++c) if (cropDef(c).tier <= s.cropTier) ++crops;
    std::string line = draw::strf("%dx%d patch", s.patchSize, s.patchSize);
    line += crops == 1 ? "  -  lettuce only" : draw::strf("  -  %d crops", crops);
    if (s.farmers > 0) line += draw::strf("  -  %d farmer%s", s.farmers, s.farmers == 1 ? "" : "s");
    return line;
}

int main(int argc, char** argv) {
    setvbuf(stdout, nullptr, _IONBF, 0);
    if (argc > 1) kEndLine = argv[1];
    Game g;
    if (!g.init()) return 1;
    SDL_CreateDirectory("saves");
    g.saves_.dir_ = "saves/"; // keep the real save slots out of it
    g.audio_.pauseDevice(true); // we pull the audio ourselves, a frame at a time
    g.settings_.musicVolume = 0.7f;
    g.audio_.setMusicVolume(0.7f);
    g.audio_.setSfxVolume(0.9f);
    SDL_Renderer* r = g.renderer();
    (void)r;
    Reel v;

    // ===== 1. Day 1 -> Day 140 ==========================================
    {
        g.newGame(0);
        g.day_ = 3; // (the caption says Day 1; this just hides the first-days hint along the bottom)
        begin(v, g, "01_day1_to_day140");
        Picker p;
        g.mouseX_ = 640; g.mouseY_ = 420;
        warm(g, p, 3.2f);
        v.top = {{"DAY 1", 12.f, pal::Cream, 64, 0}, {farmLine(g), 3.f, pal::SageMist, 196, 4}};
        v.bottom.clear();
        g.coins_ = 0;
        farmFrames(v, p, 105);
        struct Stage { int day; double budget; double coins; int frames; };
        const Stage stages[] = {{12, 70, 180, 66}, {35, 1400, 2900, 66}, {70, 9000, 21000, 66}, {140, 1e12, 260000, 96}};
        for (const Stage& s : stages) {
            fadeFrames(v, 5, true, nullptr);
            buyUpTo(g, s.budget);
            g.day_ = s.day; g.coins_ = s.coins;
            g.startDay();
            g.mouseX_ = 640; g.mouseY_ = 420; p = Picker{};
            warm(g, p, 4.f);
            v.top = {{draw::strf("DAY %d", s.day), 12.f, pal::Cream, 64, v.frame}, {farmLine(g), 3.f, pal::SageMist, 196, v.frame + 4}};
            v.bottom.clear();
            fadeFrames(v, 5, false, [&] { p.step(g); g.update(1.f / FPS); });
            farmFrames(v, p, s.frames);
        }
        fadeFrames(v, 8, true, [&] { p.step(g); g.update(1.f / FPS); });
        g.saveGame(); // so the menu's vegetable rows match this save
        g.goToMainMenu();
        endCard(v, 75);
        end(v);
    }
    // ===== 2. Every upgrade ==============================================
    {
        g.newGame(1);
        g.tree_.find("barn")->level = 1;
        buyUpTo(g, 1e12);
        g.day_ = 160; g.coins_ = 1.24e6;
        g.startDay();
        begin(v, g, "02_every_upgrade");
        v.crop = SDL_FRect{460, 130, 360, 480};
        v.cropY = 280;
        Picker p;
        p.minX = 490; p.maxX = 790; p.minY = 170; p.maxY = 590;
        g.mouseX_ = 640; g.mouseY_ = 420;
        warm(g, p, 5.f);
        v.top = {{"POV: you bought", 7.f, pal::Cream, 50, 0}, {"every upgrade", 7.f, pal::PaleGold, 130, 8}};
        fadeFrames(v, 5, false, [&] { p.step(g); g.update(1.f / FPS); });
        farmFrames(v, p, 330);
        fadeFrames(v, 8, true, [&] { p.step(g); g.update(1.f / FPS); });
        g.saveGame(); // so the menu's vegetable rows match this save
        g.goToMainMenu();
        endCard(v, 75);
        end(v);
    }
    // ===== 3. The Barn growing ==========================================
    {
        g.newGame(2);
        g.coins_ = 1e15;
        g.openTechTree();
        begin(v, g, "03_the_barn");
        v.crop = SDL_FRect{370, 64, 540, 600};
        v.cropY = 330;
        v.showCursor = false;
        v.coinCounter = false;
        auto& ts = g.treeScreen_;
        ts.mouseX_ = ts.mouseY_ = -1000.f;
        const int total = static_cast<int>(g.tree_.nodes().size());
        float zoom = ts.zoom_;
        auto camera = [&](float k) {
            float minX = 1e9f, maxX = -1e9f, minY = 1e9f, maxY = -1e9f;
            for (const auto& n : g.tree_.nodes()) {
                if (!ts.isVisible(g.tree_, n)) continue;
                minX = std::min(minX, n.gridX); maxX = std::max(maxX, n.gridX);
                minY = std::min(minY, n.gridY); maxY = std::max(maxY, n.gridY);
            }
            const float w = (maxX - minX) * 140.f + 84.f + 40.f, h = (maxY - minY) * 112.f + 84.f + 40.f;
            const float want = std::min({500.f / w, 600.f / h, 1.f});
            zoom += (want - zoom) * k;
            ts.zoom_ = zoom;
            const float wx = ts.originX_ + (minX + maxX) * 0.5f * 140.f, wy = ts.originY_ + (minY + maxY) * 0.5f * 112.f;
            const float pivotY = 88.f + 42.f;
            ts.camX_ = -(wx - 640.f) * zoom;
            ts.camY_ = 392.f - pivotY - (wy - pivotY) * zoom;
        };
        v.top = {{"THE BARN", 11.f, pal::Cream, 64, 0}, {"166 upgrades. one tiny farm.", 3.f, pal::SageMist, 190, 6}};
        auto buyOne = [&](float gain) {
            int best = -1;
            const auto& nodes = g.tree_.nodes();
            for (int i = 0; i < (int)nodes.size(); ++i)
                if (nodes[i].level == 0 && g.tree_.prereqsMet(nodes[i]) &&
                    (best < 0 || g.tree_.cost(nodes[i]) < g.tree_.cost(nodes[best])))
                    best = i;
            if (best < 0) return false;
            g.tree_.tryBuy(best, g.coins_);
            if (ts.flash_.size() != nodes.size()) ts.flash_.assign(nodes.size(), 0.f);
            ts.flash_[best] = 1.f;
            g.audio_.play(Sfx::Buy, 1.f, gain);
            return true;
        };
        auto bought = [&] { int n = 0; for (const auto& x : g.tree_.nodes()) n += x.level; return n; };
        camera(1.f);
        for (int i = 0; i < 20; ++i) {
            v.counterText = draw::strf("UPGRADES  %d / %d", bought(), total);
            g.update(1.f / FPS); camera(0.12f); frame(v);
        }
        float interval = 12.f, wait = 0.f;
        bool more = true;
        while (more) {
            wait += 1.f;
            while (wait >= interval && more) {
                wait -= interval;
                more = buyOne(interval < 2.f ? 0.45f : 0.9f);
                interval = std::max(0.5f, interval * 0.92f);
            }
            v.counterText = draw::strf("UPGRADES  %d / %d", bought(), total);
            g.update(1.f / FPS);
            camera(0.12f);
            frame(v);
        }
        for (int i = 0; i < 40; ++i) { g.update(1.f / FPS); camera(0.12f); frame(v); }
        fadeFrames(v, 8, true, [&] { g.update(1.f / FPS); });
        g.saveGame(); // so the menu's vegetable rows match this save
        g.goToMainMenu();
        endCard(v, 75);
        end(v);
    }
    std::printf("all done\n");
    std::fflush(stdout);
    _Exit(0);
}

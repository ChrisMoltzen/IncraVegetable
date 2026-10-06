// Farm.h - the garden bed and one day of picking.
//
// Vegetables are scattered naturally over a rectangular bed (not in a grid).
// Each plant takes up a circle; neighbours may overlap by at most 10% of a
// plant's width. The number of plants is patchSize x patchSize (9 to 100).
// The bed has about 25% more spots than plants; when a vegetable is picked,
// the new one sprouts in a random empty spot somewhere else.
#pragma once

#include "TechTree.h"

#include <SDL3/SDL.h>
#include <random>
#include <string>
#include <vector>

enum class Crop { Lettuce, Carrot, Pumpkin };

struct CropInfo {
    const char* name;
    double value;     // coins before multipliers
    float growMult;   // multiplier on Stats::growTime
    float pickMult;   // multiplier on Stats::pickTime
};

const CropInfo& cropInfo(Crop c);
const char* cropArtName(Crop c); // "crops/lettuce" etc.

class Farm {
public:
    void startDay(const Stats& stats, std::mt19937& rng);

    // Saving a day in progress: serialize() writes "farm ..." and "tile ..."
    // lines; restore() reads them back. restore() returns false (and leaves
    // the farm untouched) if the lines don't match the current patch size.
    std::string serialize() const;
    bool restore(const std::vector<std::string>& lines, const Stats& stats);

    // ---- Debug tools ----
    // Switches to new stats mid-day. If the patch size changed, the patch is replanted.
    void applyStats(const Stats& stats, std::mt19937& rng);
    void ripenAll();
    void setTimeLeft(float seconds);
    void setTimerFrozen(bool frozen) { timerFrozen_ = frozen; }
    void setDebugView(bool on) { debugView_ = on; }
    const Stats& stats() const { return stats_; }

    // Crops picked since the last call (used to play sounds).
    std::vector<Crop> takeHarvests();

    // Draws a ripe crop (the artist's image if there is one). Also used by the main menu.
    static void drawCrop(SDL_Renderer* r, Crop crop, float cx, float cy, float size);

    // Built-in art, used when there's no image in assets/ (see ArtCatalog.cpp).
    static void drawCropBuiltin(SDL_Renderer* r, Crop crop, float cx, float cy, float size);
    static void drawSproutBuiltin(SDL_Renderer* r, const SDL_FRect& rc);
    static void drawSoilBuiltin(SDL_Renderer* r, const SDL_FRect& rc, bool hovered); // mound under a plant
    static void drawBedBuiltin(SDL_Renderer* r, const SDL_FRect& rc, Uint32 seed);
    static void drawPickBarBuiltin(SDL_Renderer* r, const SDL_FRect& rc, bool fill);
    static void drawReachBuiltin(SDL_Renderer* r, const SDL_FRect& rc);

    // mouseX/mouseY are in game coordinates (1280x720); mouseInside is false
    // when the mouse has left the window.
    void update(float dt, float mouseX, float mouseY, bool mouseInside, double& coins, std::mt19937& rng);
    void render(SDL_Renderer* r) const;

    bool dayOver() const { return timeLeft_ <= 0.f; }
    float timeLeft() const { return timeLeft_; }
    float dayLength() const { return stats_.dayLength; }
    double earnedToday() const { return earnedToday_; }
    int pickedToday() const { return pickedToday_; }
    int pickedOf(Crop c) const { return pickedByCrop_[static_cast<int>(c)]; }

private:
    struct Tile {
        float x = 0.f, y = 0.f; // centre of the plant on screen
        int spot = -1;          // which of spots_ it's growing on
        Crop crop = Crop::Lettuce;
        float growth = 0.f; // 0..1, ripe at 1
        float pick = 0.f;   // 0..1 picking progress while hovered
        float wobble = 0.f; // random phase so ripe vegetables don't bob in sync
        float pop = 0.f;    // >0 for a moment after being replanted (sprout pop-in)
    };
    struct Particle {
        float x, y, vx, vy, life, size;
        SDL_Color color;
    };
    struct FloatText {
        float x, y, life;
        std::string text;
        SDL_Color color;
    };

    void plant(Uint32 seed); // shapes the bed, lays out the spots and puts every plant on one
    void placeOn(int index, int spot);
    void moveToFreeSpot(int index, std::mt19937& rng); // used when a picked vegetable regrows
    void sortDrawOrder();
    Crop randomCrop(std::mt19937& rng) const;
    SDL_FRect plantRect(int index) const; // square the plant is drawn in
    int plantAt(float x, float y) const;  // nearest plant under the pointer, or -1
    float pickRadius() const;
    bool tileInReach(int index, float mx, float my) const;
    void harvest(int index, double& coins, std::mt19937& rng);
    void drawVegetable(SDL_Renderer* r, const Tile& t, float cx, float cy, float size) const;

    Stats stats_;
    std::vector<Tile> tiles_;
    std::vector<Particle> particles_;
    std::vector<FloatText> floatTexts_;
    std::vector<Crop> harvests_;
    int n_ = 3;
    float plantSize_ = 70.f;               // diameter of one plant
    float bedX_ = 640.f, bedY_ = 398.f;    // centre of the bed
    float bedRX_ = 100.f, bedRY_ = 60.f;   // half-width / half-height of the bed
    Uint32 seed_ = 1;                      // decides the bed's shape and where plants go
    std::vector<SDL_FPoint> spots_;        // every place a plant can grow (more than there are plants)
    std::vector<int> drawOrder_;           // plants sorted top to bottom, so nearer ones overlap
    int hovered_ = -1;
    float timeLeft_ = 0.f;
    float clock_ = 0.f;
    double earnedToday_ = 0.0;
    int pickedToday_ = 0;
    int pickedByCrop_[3] = {0, 0, 0};
    float mouseX_ = -1000.f, mouseY_ = -1000.f;
    bool mouseInside_ = false;
    bool timerFrozen_ = false;
    bool debugView_ = false;
};

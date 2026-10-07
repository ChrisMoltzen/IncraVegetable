// Farm.h - the garden bed and one day of picking.
//
// Vegetables are scattered naturally over a rectangular bed (not in a grid).
// Each plant takes up a circle; neighbours may overlap by at most 10% of a
// plant's width. The patch has room for patchSize x patchSize plants, but
// only Stats::maxCrops of them grow at once (4 at the start; techs raise it).
// The bed has about 25% more spots than it has room for, so there are always
// gaps; when a vegetable is picked, the new one sprouts in a random empty
// spot somewhere else.
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

    // How many vegetables grow at once: maxCrops, but never more than the patch has room for.
    static int plantCount(const Stats& stats);

    // Saving a day in progress: serialize() writes "farm ..." and "tile ..."
    // lines; restore() reads them back. restore() returns false (and leaves
    // the farm untouched) if the lines don't match the current patch size.
    std::string serialize() const;
    bool restore(const std::vector<std::string>& lines, const Stats& stats);

    // ---- Debug tools ----
    // Switches to new stats mid-day. If the patch size changed, the patch is
    // replanted; if only the number of crops changed, plants are added or removed.
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
    // pose: 0 standing, 1 and 2 walking steps, 3 picking. Feet at the bottom middle of rc.
    static void drawFarmerBuiltin(SDL_Renderer* r, const SDL_FRect& rc, int pose);

    int farmerCount() const { return static_cast<int>(farmers_.size()); }

    // mouseX/mouseY are in game coordinates (1280x720); mouseInside is false
    // when the mouse has left the window.
    void update(float dt, float mouseX, float mouseY, bool mouseInside, double& coins, std::mt19937& rng);
    void render(SDL_Renderer* r) const;

    // Camera: plants are always kMaxPlantPx wide in the world, and the view
    // zooms out so the whole bed fits on screen. zoom() = how far out (1 = not at all).
    float zoom() const { return plantSize_ / 72.f; }

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
    // A hired farmer walking the patch. Picks the nearest ripe crop nobody
    // else is going for, walks to it, picks it, and looks for the next one.
    struct Farmer {
        float x = 0.f, y = 0.f;  // feet position on screen
        int target = -1;         // plant it's going for (-1 = none)
        int targetSpot = -1;     // that plant's spot when chosen (it moves when it regrows)
        float work = 0.f;        // 0..1 picking progress
        bool picking = false;    // standing at the plant, picking
        float wanderX = 0.f, wanderY = 0.f, wait = 0.f; // strolling about when nothing is ripe
        float step = 0.f;        // walk animation clock
        bool facingLeft = false;
        bool walking = false;
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
    void renderScene(SDL_Renderer* r) const;
    float viewZoom() const; // extra scale while the camera glides to a new zoom (1 = settled)
    Crop randomCrop(std::mt19937& rng) const;
    SDL_FRect plantRect(int index) const; // square the plant is drawn in
    int plantAt(float x, float y) const;  // nearest plant under the pointer, or -1
    float pickRadius() const;
    bool tileInReach(int index, float mx, float my) const;
    // Who picked a crop. Only crops you pick yourself can set off an auto-pick.
    enum class PickedBy { Player, AutoPick, Farmer };
    void harvest(int index, double& coins, std::mt19937& rng, PickedBy by = PickedBy::Player);
    void syncFarmers(std::mt19937& rng);  // adds / removes farmers to match stats_.farmers
    void updateFarmers(float dt, double& coins, std::mt19937& rng);
    SDL_FPoint farmerStand(int plant) const; // where a farmer stands to pick a plant
    SDL_FRect farmerRect(const Farmer& f) const;
    void drawFarmer(SDL_Renderer* r, const Farmer& f) const;
    float autoPickRange() const; // in pixels, centre to centre; 0 = ability not unlocked
    void drawVegetable(SDL_Renderer* r, const Tile& t, float cx, float cy, float size) const;

    Stats stats_;
    std::vector<Tile> tiles_;
    std::vector<Farmer> farmers_;
    float zoomFrom_ = 1.f, zoomT_ = 1.f;           // camera glide at the start of a day
    mutable SDL_Texture* zoomTex_ = nullptr;        // off-screen image used during the glide
    mutable SDL_Renderer* zoomTexRenderer_ = nullptr; // (freed along with the renderer)
    std::vector<Particle> particles_;
    std::vector<FloatText> floatTexts_;
    std::vector<Crop> harvests_;
    int n_ = 3;                            // patch size: room for n_ x n_ plants
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

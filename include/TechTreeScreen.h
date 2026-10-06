// TechTreeScreen.h - the between-days screen where upgrades are bought.
#pragma once

#include "TechTree.h"

#include <SDL3/SDL.h>
#include <vector>

class TechTreeScreen {
public:
    enum class Action { None, Purchased, Denied, Selected, StartDay };

    void open(const TechTree& tree);
    // `e` must already be in game coordinates (SDL_ConvertEventToRenderCoordinates).
    Action handleEvent(const SDL_Event& e, TechTree& tree, double& coins);
    void update(float dt);
    void render(SDL_Renderer* r, const TechTree& tree, double coins, int nextDay) const;

    // Built-in art, used when there's no image in assets/ (see ArtCatalog.cpp).
    static void drawBackgroundBuiltin(SDL_Renderer* r, const SDL_FRect& rc, float panX, float panY);
    static void drawNodeBuiltin(SDL_Renderer* r, const SDL_FRect& outer, SDL_Color fill, SDL_Color border);
    static void drawTooltipBuiltin(SDL_Renderer* r, const SDL_FRect& rc);
    static void drawHeaderBuiltin(SDL_Renderer* r, const SDL_FRect& rc);

private:
    SDL_FRect nodeRect(const TechNode& n) const;
    bool isVisible(const TechTree& tree, const TechNode& n) const;
    int nodeAt(const TechTree& tree, float x, float y) const;
    void renderTooltip(SDL_Renderer* r, const TechTree& tree, const TechNode& n, double coins) const;

    float camX_ = 0.f, camY_ = 0.f;
    float mouseX_ = 0.f, mouseY_ = 0.f;
    bool pressing_ = false, dragged_ = false;
    float pressX_ = 0.f, pressY_ = 0.f;
    float clock_ = 0.f;
    bool touchMode_ = false; // last input came from a finger
    int selected_ = -1;      // node picked with a tap (touch only)
    std::vector<float> flash_; // per-node purchase flash, fades from 1 to 0
    SDL_FRect startButton_{1280.f - 330.f, 720.f - 86.f, 300.f, 60.f};
};

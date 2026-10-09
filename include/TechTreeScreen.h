// TechTreeScreen.h - the between-days screen where upgrades are bought.
#pragma once

#include "TechTree.h"

#include <SDL3/SDL.h>
#include <string>
#include <vector>

class TechTreeScreen {
public:
    enum class Action { None, Purchased, Denied, Selected, StartDay };

    void open(const TechTree& tree);
    // `e` must already be in game coordinates (SDL_ConvertEventToRenderCoordinates).
    Action handleEvent(const SDL_Event& e, TechTree& tree, double& coins);
    void update(float dt);
    void clearHover() { mouseX_ = mouseY_ = -1000.f; selected_ = -1; } // no tooltip (under a pop-up)
    void render(SDL_Renderer* r, const TechTree& tree, double coins, int nextDay) const;

    // Built-in art, used when there's no image in assets/ (see ArtCatalog.cpp).
    static void drawNodeBuiltin(SDL_Renderer* r, const SDL_FRect& outer, SDL_Color fill, SDL_Color border);
    // Picture for a tech's tile when there's no assets/tree/icons/<id>.png.
    // The shipped techs have their own; any other tech gets its initials.
    static void drawIconBuiltin(SDL_Renderer* r, const SDL_FRect& rc, const std::string& id, const std::string& name);
    static void drawTooltipBuiltin(SDL_Renderer* r, const SDL_FRect& rc);
    static void drawHeaderBuiltin(SDL_Renderer* r, const SDL_FRect& rc);

private:
    SDL_FRect nodeRect(const TechNode& n) const;
    void fitView(const TechTree& tree); // zoom and pan so every visible tech is on screen
    // Zoom: tiles are laid out at zoom 1, then scaled about the pivot (the
    // middle of the screen, horizontally, and the top row of tiles).
    SDL_FPoint toScreen(float x, float y) const;
    void zoomAt(float sx, float sy, float newZoom); // keeps the point under (sx, sy) still
    SDL_FRect zoomInButton() const;
    SDL_FRect zoomOutButton() const;
    bool isVisible(const TechTree& tree, const TechNode& n) const;
    int nodeAt(const TechTree& tree, float x, float y) const;
    void renderTooltip(SDL_Renderer* r, const TechTree& tree, const TechNode& n, double coins) const;

    float originX_ = 640.f, originY_ = 300.f; // screen position of grid (0,0): set so the tree is centred
    float camX_ = 0.f, camY_ = 0.f;
    float zoom_ = 1.f;
    // Two-finger pinch (touch): the fingers down, and the spread when it started.
    SDL_FingerID fingerA_ = 0, fingerB_ = 0;
    SDL_FPoint fingerPosA_{}, fingerPosB_{};
    int fingers_ = 0;
    bool pinching_ = false;
    int pressedZoom_ = 0; // +1 / -1 while a zoom button is held
    float mouseX_ = 0.f, mouseY_ = 0.f;
    bool pressing_ = false, dragged_ = false;
    float pressX_ = 0.f, pressY_ = 0.f;
    float clock_ = 0.f;
    bool touchMode_ = false; // last input came from a finger
    int selected_ = -1;      // node picked with a tap (touch only)
    std::vector<float> flash_; // per-node purchase flash, fades from 1 to 0
    SDL_FRect startButton_{1280.f - 330.f, 720.f - 86.f, 300.f, 60.f};
};

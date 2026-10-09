// CompleteScreen.h - the "you've done it" pop-up, shown once per farm when the
// last upgrade in The Barn is bought (in the demo: the last one the demo allows).
// Vegetables and coins tumble down behind it; the player can carry on farming
// or go back to the main menu.
#pragma once

#include "UI.h"

#include <SDL3/SDL.h>
#include <string>
#include <vector>

class CompleteScreen {
public:
    enum class Action { None, KeepPlaying, MainMenu };

    // title: the big line; message: the line under it; facts: a few small lines (days, coins...).
    void open(const std::string& title, const std::string& message, std::vector<std::string> facts);
    Action handleEvent(const SDL_Event& e);
    void update(float dt);
    void render(SDL_Renderer* r) const;

private:
    struct Bit { // one falling vegetable or coin
        float x, y, vy, spin, size;
        int kind; // -1 = coin, else a crop
    };
    std::string title_, message_;
    std::vector<std::string> facts_;
    std::vector<Bit> bits_;
    ui::ButtonList buttons_;
    float time_ = 0.f;
    Uint32 seed_ = 1;
    float rnd();
};

// StatsScreen.h - the stats page, opened from the chart button in the top bar
// on the farm and in the barn. Drawn over the (dimmed) scene it came from.
#pragma once

#include "UI.h"

#include <SDL3/SDL.h>
#include <string>
#include <vector>

class StatsScreen {
public:
    struct Row {
        std::string label, value;
        bool heading = false; // a small section title instead of a stat
        bool indent = false;  // a breakdown of the row above
    };

    void open(std::vector<Row> rows);
    void setRows(std::vector<Row> rows) { rows_ = std::move(rows); } // refresh while open
    bool handleEvent(const SDL_Event& e); // true = close
    void render(SDL_Renderer* r) const;

    // The chart button in the top bar.
    static void drawButton(SDL_Renderer* r, const SDL_FRect& rc, bool hovered);
    static void drawButtonBuiltin(SDL_Renderer* r, const SDL_FRect& rc, bool hovered);

private:
    std::vector<Row> rows_;
    ui::ButtonList buttons_;
};

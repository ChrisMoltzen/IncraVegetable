// ArtCatalog.cpp - every piece of artwork the game uses.
//
// Each entry is an image name (= file path inside assets/, without the
// extension), its recommended size, a description for ART_LIST.md, and the
// built-in drawing used when no image is provided. To give something new in
// the game replaceable art: add an entry here and draw it with art::draw().

#include "Art.h"
#include "DebugMenu.h"
#include "Draw.h"
#include "Palette.h"
#include "Farm.h"
#include "Menus.h"
#include "TechData.h"
#include "TechTreeScreen.h"
#include "UI.h"

#include <algorithm>
#include <cctype>
#include <cmath>

// Entries below leave the last Info field (templateDraw) out on purpose.
#if defined(__GNUC__)
#pragma GCC diagnostic ignored "-Wmissing-field-initializers"
#endif

namespace art {

namespace {

// A field of grass tufts, the same every time.
void drawGrassBuiltin(SDL_Renderer* r, const SDL_FRect& rc) {
    draw::fillRect(r, rc.x, rc.y, rc.w, rc.h, SDL_Color{86, 150, 70, 255});
    Uint32 seed = 12345u;
    auto rnd = [&seed] {
        seed = seed * 1664525u + 1013904223u;
        return static_cast<float>(seed >> 8) / static_cast<float>(1u << 24);
    };
    const SDL_Color c{70, 128, 56, 255};
    int count = static_cast<int>(160.f * rc.w * rc.h / (1280.f * 720.f)) + 1;
    for (int i = 0; i < count; ++i) {
        float x = rc.x + rnd() * rc.w, y = rc.y + 70.f / 720.f * rc.h + rnd() * rc.h * (650.f / 720.f), s = 0.6f + rnd() * 0.8f;
        draw::fillTriangle(r, {x - 5 * s, y}, {x - 1 * s, y}, {x - 6 * s, y - 10 * s}, c);
        draw::fillTriangle(r, {x - 2 * s, y}, {x + 2 * s, y}, {x, y - 13 * s}, c);
        draw::fillTriangle(r, {x + 1 * s, y}, {x + 5 * s, y}, {x + 6 * s, y - 9 * s}, c);
    }
}

void drawHudLogoBuiltin(SDL_Renderer* r, const SDL_FRect& rc) {
    float scale = std::min(rc.h / 9.f, rc.w / draw::textWidth("IncraVegetable", 1.f));
    float y = rc.y + (rc.h - 8.f * scale) * 0.5f;
    draw::textShadow(r, rc.x, y, "Incra", scale, pal::FreshLeaf);
    draw::textShadow(r, rc.x + draw::textWidth("Incra", scale), y, "Vegetable", scale, pal::Pumpkin);
}

void drawPanelHeaderBuiltin(SDL_Renderer* r, const SDL_FRect& rc) {
    const SDL_Color orange{240, 150, 60, 255};
    draw::fillRoundRect(r, rc.x, rc.y, rc.w, rc.h, 20, orange);
    draw::fillRect(r, rc.x, rc.y + rc.h * 0.7f, rc.w, rc.h * 0.3f, orange); // flat bottom edge
}

void drawFontTemplate(SDL_Renderer* r, const SDL_FRect& rc) {
    // The built-in font, 16 characters across and 6 down, starting at the space.
    float cw = rc.w / 16.f, ch = rc.h / 6.f;
    float scale = std::min(cw, ch) / 8.f;
    SDL_SetRenderDrawColor(r, 255, 255, 255, 255);
    SDL_SetRenderScale(r, scale, scale);
    for (int i = 0; i < 96; ++i) {
        char s[2] = {static_cast<char>(32 + i), 0};
        SDL_RenderDebugText(r, (rc.x + (i % 16) * cw) / scale, (rc.y + (i / 16) * ch) / scale, s);
    }
    SDL_SetRenderScale(r, 1.f, 1.f);
}

void addButtonStyle(ui::Style style, const char* what) {
    std::string base = ui::artName(style);
    struct V {
        const char* suffix;
        ui::ButtonState state;
        const char* desc;
    };
    const V variants[] = {
        {"", ui::ButtonState::Normal, ""},
        {"_hover", ui::ButtonState::Hover, " - mouse over it (optional)"},
        {"_pressed", ui::ButtonState::Pressed, " - being pressed (optional)"},
        {"_disabled", ui::ButtonState::Disabled, " - can't be used right now (optional)"},
    };
    for (const auto& v : variants) {
        add({base + v.suffix, 256, 80, std::string(what) + v.desc, true,
             [style, st = v.state](SDL_Renderer* r, const SDL_FRect& rc) { ui::drawButtonBuiltin(r, rc, style, st); }});
    }
}

void addNodeStyle(const char* name, SDL_Color fill, SDL_Color border, const char* what) {
    add({name, 96, 96, what, true, [fill, border](SDL_Renderer* r, const SDL_FRect& rc) {
             TechTreeScreen::drawNodeBuiltin(r, rc, fill, border);
         }});
}

} // namespace

void registerCatalog(const std::vector<std::pair<std::string, std::string>>& techNodes) {
    // ---------------- Farm ----------------
    add({"farm/background", 1280, 720, "Everything behind the vegetable patch while farming", false, drawGrassBuiltin});
    add({"farm/hud_bar", 1280, 72, "Strip along the top of the farm screen (title, timer, coins sit on it)", false,
         [](SDL_Renderer* r, const SDL_FRect& rc) { draw::fillRect(r, rc.x, rc.y, rc.w, rc.h, SDL_Color{20, 28, 18, 210}); }});
    add({"farm/bed", 640, 400,
         "The rectangular garden bed the vegetables grow in. Stretched to fit, always 1.6x wider than tall. Plants "
         "sit inside the middle ~90%, so leave a border of soil or edging around them",
         false, [](SDL_Renderer* r, const SDL_FRect& rc) { Farm::drawBedBuiltin(r, rc, 1234u); }});
    // Fence around the bed. Tiles are about two-thirds of a plant across and get stretched a little
    // (never more than ~10%) so the sides meet at the corners; they join up edge to edge.
    add({"farm/fence_h", 64, 64, "Fence tile along the top and bottom of the bed (joins left and right)", false,
         [](SDL_Renderer* r, const SDL_FRect& rc) { Farm::drawFenceBuiltin(r, rc, Farm::FencePiece::Horizontal); }});
    add({"farm/fence_v", 64, 64, "Fence tile down the left and right sides of the bed (joins top and bottom)", false,
         [](SDL_Renderer* r, const SDL_FRect& rc) { Farm::drawFenceBuiltin(r, rc, Farm::FencePiece::Vertical); }});
    add({"farm/fence_corner", 64, 64, "Fence corner post (all four corners of the fence)", false,
         [](SDL_Renderer* r, const SDL_FRect& rc) { Farm::drawFenceBuiltin(r, rc, Farm::FencePiece::Corner); }});
    add({"farm/fence_gate", 64, 64, "Gate in the middle of the bottom fence (joins left and right like fence_h)", false,
         [](SDL_Renderer* r, const SDL_FRect& rc) { Farm::drawFenceBuiltin(r, rc, Farm::FencePiece::Gate); }});
    add({"farm/soil", 128, 128, "Mound of soil under each vegetable (sits in the lower part of the plant's square)", false,
         [](SDL_Renderer* r, const SDL_FRect& rc) { Farm::drawSoilBuiltin(r, rc, false); }});
    add({"farm/soil_hover", 128, 128, "Mound under the plant the pointer is on (optional)", false,
         [](SDL_Renderer* r, const SDL_FRect& rc) { Farm::drawSoilBuiltin(r, rc, true); }});

    // ---------------- Crops ----------------
    for (Crop c = 0; c < cropCount(); ++c) {
        std::string name = cropArtName(c);
        std::string label = cropDef(c).name;
        add({name, 128, 128, "Ripe " + label + ", filling the square one plant takes up (neighbours may overlap it by up to 10%)", false,
             [c](SDL_Renderer* r, const SDL_FRect& rc) {
                 Farm::drawCropBuiltin(r, c, rc.x + rc.w * 0.5f, rc.y + rc.h * 0.5f, rc.w);
             }});
    }
    add({"crops/sprout", 128, 128,
         "Growing plant, drawn small when just planted and full size just before it ripens. Keep the stem's base about "
         "2/3 of the way down",
         false, Farm::drawSproutBuiltin});
    for (Crop c = 0; c < cropCount(); ++c) {
        std::string label = cropDef(c).name;
        add({cropArtName(c) + "_sprout", 128, 128,
             "Growing " + label + " (optional - otherwise crops/sprout is used)", false, Farm::drawSproutBuiltin});
    }

    // ---------------- Effects ----------------
    // Farmers (Hired Hand upgrade). Only farm/farmer is needed; the others are optional poses.
    const char* farmerSize = " Feet at the bottom middle, about 8% up from the bottom edge. Draw it facing RIGHT: "
                             "it's mirrored when the farmer walks left.";
    add({"farm/farmer", 128, 128, std::string("Hired farmer, standing.") + farmerSize, false,
         [](SDL_Renderer* r, const SDL_FRect& rc) { Farm::drawFarmerBuiltin(r, rc, 0); }});
    add({"farm/farmer_walk1", 128, 128, std::string("Farmer walking, first step (optional - else farm/farmer bobs).") + farmerSize,
         false, [](SDL_Renderer* r, const SDL_FRect& rc) { Farm::drawFarmerBuiltin(r, rc, 1); }});
    add({"farm/farmer_walk2", 128, 128, std::string("Farmer walking, second step (optional).") + farmerSize, false,
         [](SDL_Renderer* r, const SDL_FRect& rc) { Farm::drawFarmerBuiltin(r, rc, 2); }});
    add({"farm/farmer_pick", 128, 128, std::string("Farmer bending down to pick a crop (optional).") + farmerSize, false,
         [](SDL_Renderer* r, const SDL_FRect& rc) { Farm::drawFarmerBuiltin(r, rc, 3); }});
    add({"fx/particle", 32, 32, "Burst particle when a vegetable is picked. Draw it WHITE: the game tints it to the crop's colour",
         false, [](SDL_Renderer* r, const SDL_FRect& rc) {
             draw::fillCircle(r, rc.x + rc.w * 0.5f, rc.y + rc.h * 0.5f, rc.w * 0.45f, SDL_Color{255, 255, 255, 255});
         }});
    add({"fx/reach_circle", 256, 256, "Picking area around the pointer (Wide Reach upgrade). Usually see-through", false,
         Farm::drawReachBuiltin});

    // ---------------- HUD ----------------
    add({"ui/hud_logo", 336, 30, "Game name in the top-left of the farm screen", false, drawHudLogoBuiltin});
    add({"ui/coin", 64, 64, "Coin icon", false, ui::drawCoinBuiltin});
    add({"ui/dial_sky", 256, 256,
         "Day/night dial: the WHOLE sky disc. Day half on top with the sun at the top middle, night half below with "
         "the moon at the bottom middle. The game turns it so the sun rises on the left and sets on the right; only "
         "the top half shows",
         false, ui::drawDialSkyBuiltin});
    add({"ui/dial_frame", 280, 150,
         "Day/night dial: frame drawn over the sky disc. Leave the half-circle window see-through (it fills the frame's "
         "width minus about 6% each side, with the horizon about 79% of the way down); the bottom strip hides the sun "
         "as it sets",
         false, ui::drawDialFrameBuiltin});
    add({"ui/pick_bar_back", 128, 16, "Behind the picking progress bar under a vegetable", false,
         [](SDL_Renderer* r, const SDL_FRect& rc) { Farm::drawPickBarBuiltin(r, rc, false); }});
    add({"ui/pick_bar_fill", 128, 16, "Picking progress bar when full (cropped while picking)", false,
         [](SDL_Renderer* r, const SDL_FRect& rc) { Farm::drawPickBarBuiltin(r, rc, true); }});
    add({"ui/pause_button", 96, 96, "Pause button, top-right while playing", false,
         [](SDL_Renderer* r, const SDL_FRect& rc) { ui::drawPauseIconBuiltin(r, rc, false); }});
    add({"ui/pause_button_hover", 96, 96, "Pause button with the mouse over it (optional)", false,
         [](SDL_Renderer* r, const SDL_FRect& rc) { ui::drawPauseIconBuiltin(r, rc, true); }});
    add({"ui/debug_button", 80, 80, "Debug screen button, bottom-left", false,
         [](SDL_Renderer* r, const SDL_FRect& rc) { DebugMenu::drawOpenButtonBuiltin(r, rc, false); }});
    add({"ui/debug_button_hover", 80, 80, "Debug button with the mouse over it (optional)", false,
         [](SDL_Renderer* r, const SDL_FRect& rc) { DebugMenu::drawOpenButtonBuiltin(r, rc, true); }});

    // ---------------- Buttons, panels, sliders ----------------
    addButtonStyle(ui::Style::Primary, "Main orange button (New Game, Start Day...)");
    addButtonStyle(ui::Style::Secondary, "Green button (Continue, Resume, The Barn...)");
    addButtonStyle(ui::Style::Danger, "Red button (Overwrite, Quit Game...)");
    addButtonStyle(ui::Style::Ghost, "Grey button (Settings, Back...)");
    add({"ui/panel", 256, 256, "Pop-up box behind menus (pause, settings, day summary, confirm)", true, ui::drawPanelBuiltin});
    add({"ui/panel_header", 560, 70, "Coloured title strip at the top of the day summary", false, drawPanelHeaderBuiltin});
    add({"ui/tooltip", 128, 128, "Box behind upgrade descriptions in The Barn", true, TechTreeScreen::drawTooltipBuiltin});
    add({"ui/slider_track", 300, 14, "Empty volume slider", false,
         [](SDL_Renderer* r, const SDL_FRect& rc) { SettingsMenu::drawSliderBarBuiltin(r, rc, false); }});
    add({"ui/slider_fill", 300, 14, "Full volume slider (cropped to the volume)", false,
         [](SDL_Renderer* r, const SDL_FRect& rc) { SettingsMenu::drawSliderBarBuiltin(r, rc, true); }});
    add({"ui/slider_knob", 64, 64, "Handle you drag on a volume slider", false, SettingsMenu::drawKnobBuiltin});
    add({"ui/font", 256, 96,
         "Optional font: 16 x 6 grid of equal-sized characters, ASCII 32 (space) to 127 in order, drawn WHITE "
         "(the game colours them). Leave it out to keep the built-in font",
         false, nullptr, drawFontTemplate});

    // ---------------- Menus ----------------
    add({"menu/background", 1280, 720, "Main menu background (vegetable rows drift over it; turn them off in art.txt)", false,
         MainMenu::drawBackgroundBuiltin});
    add({"menu/logo", 900, 112, "Game title on the main menu", false, MainMenu::drawLogoBuiltin});
    add({"menu/slots_background", 1280, 720, "Background of the New Game / Load Game slot screen", false,
         SlotMenu::drawBackgroundBuiltin});

    // ---------------- Tech tree ----------------
    add({"tree/background", 1280, 720, "Background of The Barn (tech tree)", false,
         [](SDL_Renderer* r, const SDL_FRect& rc) { TechTreeScreen::drawBackgroundBuiltin(r, rc, 0.f, 0.f); }});
    add({"tree/header_bar", 1280, 64, "Strip along the top of The Barn (tech tree)", false, TechTreeScreen::drawHeaderBuiltin});
    addNodeStyle("tree/node", {44, 50, 56, 255}, {120, 128, 135, 255}, "Upgrade you can't afford yet");
    addNodeStyle("tree/node_affordable", {32, 72, 38, 255}, {130, 220, 120, 255}, "Upgrade you can buy now");
    addNodeStyle("tree/node_locked", {32, 33, 37, 255}, {70, 72, 78, 255}, "Upgrade whose requirements aren't met");
    addNodeStyle("tree/node_maxed", {85, 68, 25, 255}, {255, 205, 70, 255}, "Fully upgraded");
    // "_hover" versions of the tree nodes are optional; without them the node is brightened.
    // Tech icons: every built-in picture (so any tech can use it, and you can
    // repaint it), plus any other icon name a tech uses.
    for (const auto& b : techdata::builtinIcons()) {
        std::string key = b.key;
        std::string usedBy;
        for (const auto& [icon, title] : techNodes)
            if (icon == key) usedBy += (usedBy.empty() ? "" : ", ") + title;
        add({"tree/icons/" + key, 64, 64,
             "Tech icon '" + key + "' (" + b.label + ")" + (usedBy.empty() ? "" : " - used by " + usedBy),
             false, [key](SDL_Renderer* r, const SDL_FRect& rc) { TechTreeScreen::drawIconBuiltin(r, rc, key, key); }});
    }
    // Every crop can be a tech icon too ("crop_<id>"), drawn like the crop itself.
    for (Crop c = 0; c < cropCount(); ++c) {
        std::string id = cropDef(c).id, label = cropDef(c).name;
        add({"tree/icons/crop_" + id, 64, 64, "Tech icon 'crop_" + id + "' (the " + label + " crop)", false,
             [id](SDL_Renderer* r, const SDL_FRect& rc) { TechTreeScreen::drawIconBuiltin(r, rc, "crop_" + id, id); }});
    }
    for (const auto& [icon, title] : techNodes) {
        std::string i = icon, t = title;
        add({"tree/icons/" + icon, 64, 64, "Tech icon '" + icon + "' for the '" + title + "' upgrade", false,
             [i, t](SDL_Renderer* r, const SDL_FRect& rc) { TechTreeScreen::drawIconBuiltin(r, rc, i, t); }});
    }
}

} // namespace art

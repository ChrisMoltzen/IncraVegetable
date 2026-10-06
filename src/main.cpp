// IncraVegetable - an incremental farming game.
// Hover over ripe vegetables to pick them before sunset, then spend your
// coins in the tech tree to grow your farm.
//
// Uses SDL's "main callbacks": instead of owning the main loop, the game
// gives SDL four functions to call. This is the way SDL recommends for iOS
// (where the OS owns the loop) and works the same on Windows, macOS and Linux.

#define SDL_MAIN_USE_CALLBACKS 1
#include <SDL3/SDL.h>
#include <SDL3/SDL_main.h>

#include "Game.h"

#include <string>

SDL_AppResult SDL_AppInit(void** appstate, int argc, char* argv[]) {
    auto* game = new Game();
    if (!game->init()) {
        delete game;
        return SDL_APP_FAILURE;
    }
    *appstate = game;

    // IncraVegetable --export-art-templates [folder]
    // Writes every built-in picture as a PNG to paint over, then exits.
    for (int i = 1; i < argc; ++i) {
        if (std::string(argv[i]) == "--export-art-templates") {
            std::string folder = (i + 1 < argc) ? argv[i + 1] : "art_templates";
            int n = game->exportArtTemplates(folder);
            SDL_Log("Wrote %d art templates (and ART_LIST.md) to %s", n, folder.c_str());
            return SDL_APP_SUCCESS;
        }
    }
    return SDL_APP_CONTINUE;
}

SDL_AppResult SDL_AppEvent(void* appstate, SDL_Event* event) {
    auto* game = static_cast<Game*>(appstate);
    game->handleEvent(*event);
    return game->running() ? SDL_APP_CONTINUE : SDL_APP_SUCCESS;
}

SDL_AppResult SDL_AppIterate(void* appstate) {
    auto* game = static_cast<Game*>(appstate);
    game->iterate();
    return game->running() ? SDL_APP_CONTINUE : SDL_APP_SUCCESS;
}

void SDL_AppQuit(void* appstate, SDL_AppResult /*result*/) {
    auto* game = static_cast<Game*>(appstate);
    if (!game) return;
    game->shutdown();
    delete game;
}

// TechTreeEditor - design IncraVegetable's tech tree.
//
//   make editor            (from the project folder)
//   build/techtree/TechTreeEditor   [optional path to TechTreeData.h]
//
// It edits include/TechTreeData.h, which the game compiles in.

#include <SDL3/SDL.h>
#include <SDL3/SDL_main.h>

#include "Editor.h"

int main(int argc, char* argv[]) {
    Editor editor;
    if (!editor.init(argc, argv)) return 1;
    while (editor.running()) {
        SDL_Event e;
        while (SDL_PollEvent(&e)) editor.event(e);
        editor.frame();
    }
    editor.shutdown();
    return 0;
}

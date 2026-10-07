PROJECTNAME = game
OUTPUT_DIR = build

RPATH = -rpath /Library/Frameworks
FRAMEWORK = -F/Library/Frameworks
FRAMEWORK_BIN = -framework sdl3
INCLUDE_DIRS = -Iinclude
# Load art from this project's assets/ folder first, wherever the game is run
# from, so an old copy (e.g. build/assets) can't hide your latest art.
ASSETS_DEF = -DINCRA_SOURCE_ASSETS_DIR=\"$(CURDIR)/assets\"

SRC = $(wildcard src/*.cpp)
# Optimised builds: without -O2 the game and editor run several times slower.
OPT = -O2

default:
	clang++ $(SRC) -o $(OUTPUT_DIR)/$(PROJECTNAME) $(INCLUDE_DIRS) $(ASSETS_DEF) $(RPATH) $(FRAMEWORK) $(FRAMEWORK_BIN) -std=c++20 $(OPT)

# Tech tree editor: make editor, then run build/TechTreeEditor
EDITOR_SRC = $(wildcard tools/TechTreeEditor/src/*.cpp) src/TechData.cpp src/CropLooks.cpp src/TileShapes.cpp

editor:
	clang++ $(EDITOR_SRC) -o $(OUTPUT_DIR)/TechTreeEditor -Itools/TechTreeEditor/include $(INCLUDE_DIRS) $(RPATH) $(FRAMEWORK) $(FRAMEWORK_BIN) -std=c++20 $(OPT)

# ---------------------------------------------------------------------------
# Windows: 64-bit MinGW g++ (MSYS2 UCRT64) with the SDL3 Visual C++ download
# unzipped to C:/SDL (so C:/SDL/include and C:/SDL/lib/x64 exist).
#   make win          builds build/game.exe
#   make editor-win   builds build/TechTreeEditor.exe
# Both copy the 64-bit SDL3.dll into build/ so the .exe runs straight away.
# SDL somewhere else? Run e.g.  make win SDL_DIR=D:/libs/SDL3
#
# The C++ runtime is built into the .exe (-static), so it doesn't need MSYS2's
# DLLs and can't pick up a 32-bit copy from PATH (error 0xc000007b). SDL is
# the exception: -Bdynamic links it through SDL3.lib to SDL3.dll, then
# -Bstatic switches back for the libraries g++ adds after it.
# ---------------------------------------------------------------------------
SDL_DIR = C:/SDL
WIN_INCLUDE_DIRS = -Iinclude -I$(SDL_DIR)/include
WIN_LIBS = -static -L$(SDL_DIR)/lib/x64 -Wl,-Bdynamic -lSDL3 -Wl,-Bstatic
# PowerShell copies the DLL: it works whether make runs commands through
# cmd.exe or through MSYS2's sh.
WIN_COPY_DLL = powershell -NoProfile -Command "Copy-Item -Force '$(SDL_DIR)/lib/x64/SDL3.dll' '$(OUTPUT_DIR)/'"

win:
	g++ $(SRC) -o $(OUTPUT_DIR)/$(PROJECTNAME).exe $(WIN_INCLUDE_DIRS) $(ASSETS_DEF) $(WIN_LIBS) -std=c++20 $(OPT)
	$(WIN_COPY_DLL)

editor-win:
	g++ $(EDITOR_SRC) -o $(OUTPUT_DIR)/TechTreeEditor.exe -Itools/TechTreeEditor/include $(WIN_INCLUDE_DIRS) $(WIN_LIBS) -std=c++20 $(OPT)
	$(WIN_COPY_DLL)

.PHONY: default editor win editor-win

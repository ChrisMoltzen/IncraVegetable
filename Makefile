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

# Tech tree editor: make editor, then run build/techtree/TechTreeEditor.
# It gets its own folder (with its own SDL3.dll on Windows), so the game can be
# rebuilt while the editor is open.
EDITOR_SRC = $(wildcard tools/TechTreeEditor/src/*.cpp) src/TechData.cpp src/CropLooks.cpp src/TileShapes.cpp
EDITOR_DIR = $(OUTPUT_DIR)/techtree

editor:
	mkdir -p $(EDITOR_DIR)
	clang++ $(EDITOR_SRC) -o $(EDITOR_DIR)/TechTreeEditor -Itools/TechTreeEditor/include $(INCLUDE_DIRS) $(RPATH) $(FRAMEWORK) $(FRAMEWORK_BIN) -std=c++20 $(OPT)

# ---------------------------------------------------------------------------
# Windows: 64-bit MinGW g++ (MSYS2 UCRT64) with the SDL3 Visual C++ download
# unzipped to C:/SDL (so C:/SDL/include and C:/SDL/lib/x64 exist).
#   make win          builds build/game.exe
#   make editor-win   builds build/techtree/TechTreeEditor.exe
# Both copy the 64-bit SDL3.dll next to their .exe so it runs straight away.
# The editor has its own folder and its own copy of the DLL, so you can run
# make win while the editor is open (Windows won't overwrite a DLL in use).
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
WIN_EDITOR_DIR = powershell -NoProfile -Command "New-Item -ItemType Directory -Force '$(EDITOR_DIR)' | Out-Null"
WIN_COPY_DLL_EDITOR = powershell -NoProfile -Command "Copy-Item -Force '$(SDL_DIR)/lib/x64/SDL3.dll' '$(EDITOR_DIR)/'"

# The .exe's own icon (Explorer, desktop shortcuts): icon/IncraVegetable.ico, compiled in with windres.
WIN_ICON_RES = $(OUTPUT_DIR)/IncraVegetable_icon.o
WIN_ICON = windres -I icon icon/IncraVegetable.rc -O coff -o $(WIN_ICON_RES)

win:
	$(WIN_ICON)
	g++ $(SRC) $(WIN_ICON_RES) -o $(OUTPUT_DIR)/$(PROJECTNAME).exe $(WIN_INCLUDE_DIRS) $(ASSETS_DEF) $(WIN_LIBS) -std=c++20 $(OPT)
	$(WIN_COPY_DLL)

editor-win:
	$(WIN_EDITOR_DIR)
	g++ $(EDITOR_SRC) -o $(EDITOR_DIR)/TechTreeEditor.exe -Itools/TechTreeEditor/include $(WIN_INCLUDE_DIRS) $(WIN_LIBS) -std=c++20 $(OPT)
	$(WIN_COPY_DLL_EDITOR)

# ---------------------------------------------------------------------------
# Release builds: every file in assets/ is compiled into the game, and it never
# reads an assets folder, so players can't swap the art or sounds. The debug
# screen (F1 / bug button) and F5 reload are left out, and saves must be the
# scrambled kind. Everything goes in build/release/.
#   make release        macOS: build/release/game
#   make release-win    Windows: build/release/game.exe (+ SDL3.dll)
# tools/AssetPacker (built and run first) turns assets/ into AssetPack.cpp.
# ---------------------------------------------------------------------------
RELEASE_DIR = $(OUTPUT_DIR)/release
RELEASE_DEFS = -DINCRA_RELEASE -DINCRA_PACKED_ASSETS -DINCRA_DEBUG_TOOLS=0
PACKER_SRC = tools/AssetPacker/main.cpp

release:
	mkdir -p $(RELEASE_DIR)
	clang++ $(PACKER_SRC) -o $(RELEASE_DIR)/AssetPacker $(INCLUDE_DIRS) -std=c++20 -O2
	$(RELEASE_DIR)/AssetPacker assets $(RELEASE_DIR)/AssetPack.cpp
	clang++ $(SRC) $(RELEASE_DIR)/AssetPack.cpp -o $(RELEASE_DIR)/$(PROJECTNAME) $(INCLUDE_DIRS) $(RELEASE_DEFS) $(RPATH) $(FRAMEWORK) $(FRAMEWORK_BIN) -std=c++20 $(OPT)

WIN_RELEASE_DIR = powershell -NoProfile -Command "New-Item -ItemType Directory -Force '$(RELEASE_DIR)' | Out-Null"
WIN_RUN_PACKER = powershell -NoProfile -Command "& './$(RELEASE_DIR)/AssetPacker.exe' assets '$(RELEASE_DIR)/AssetPack.cpp'"
WIN_COPY_DLL_RELEASE = powershell -NoProfile -Command "Copy-Item -Force '$(SDL_DIR)/lib/x64/SDL3.dll' '$(RELEASE_DIR)/'"

release-win:
	$(WIN_RELEASE_DIR)
	g++ $(PACKER_SRC) -o $(RELEASE_DIR)/AssetPacker.exe $(INCLUDE_DIRS) -static -std=c++20 -O2
	$(WIN_RUN_PACKER)
	$(WIN_ICON)
	g++ $(SRC) $(RELEASE_DIR)/AssetPack.cpp $(WIN_ICON_RES) -o $(RELEASE_DIR)/$(PROJECTNAME).exe $(WIN_INCLUDE_DIRS) $(RELEASE_DEFS) $(WIN_LIBS) -std=c++20 $(OPT) -mwindows
	$(WIN_COPY_DLL_RELEASE)

# ---------------------------------------------------------------------------
# Short vertical videos for Instagram / TikTok (see tools/Reels/main.cpp).
# Needs ffmpeg on your PATH. The videos land in build/reels/.
#   make reels                     the end card says "Play it on itch.io"
#   make reels LINE="Out now!"     ...or whatever you like
# ---------------------------------------------------------------------------
REELS_DIR = $(OUTPUT_DIR)/reels
LINE ?= Play it on itch.io

reels:
	mkdir -p $(REELS_DIR)
	clang++ $(filter-out src/main.cpp,$(SRC)) tools/Reels/main.cpp -o $(REELS_DIR)/Reels $(INCLUDE_DIRS) $(ASSETS_DEF) -DINCRA_DEBUG_TOOLS=0 $(RPATH) $(FRAMEWORK) $(FRAMEWORK_BIN) -std=c++20 $(OPT)
	cd $(REELS_DIR) && SDL_AUDIODRIVER=dummy ./Reels "$(LINE)"

.PHONY: default editor win editor-win release release-win reels

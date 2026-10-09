# The game's file name (build/IncraVegetable, IncraVegetable.exe). Demo builds
# get " Demo" on the end: "IncraVegetable Demo" / "IncraVegetable Demo.exe".
PROJECTNAME = IncraVegetable
DEMO_NAME = $(PROJECTNAME) Demo
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
#   make win          builds build/IncraVegetable.exe
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
#   make release        macOS: build/release/IncraVegetable
#   make release-win    Windows: build/release/IncraVegetable.exe (+ SDL3.dll)
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

.PHONY: default editor win editor-win release release-win

# ---------------------------------------------------------------------------
# Demo builds: the whole game, but only the techs within DEMO_RADIUS of The
# Barn can be bought (the rest show with a padlock, past a dotted ring).
# DEMO_RADIUS is in tech tree rows; one step along an arm is about 1.2 rows.
# The default, 6, gives 55 of the 166 techs: carrots, pumpkins, farmhands and
# the first Helping Hand upgrades, about 45 minutes of play.
#   make demo                  macOS dev build:      "build/demo/IncraVegetable Demo"
#   make demo-win              Windows dev build:    "build\demo\IncraVegetable Demo.exe"
#   make demo-release          macOS release:        "build/demo-release/IncraVegetable Demo"
#   make demo-release-win      Windows release:      "build\demo-release\IncraVegetable Demo.exe"
#   make demo DEMO_RADIUS=4    ...a smaller demo (any of the four)
# Saves are shared with the full game, so a demo farm carries straight over.
# ---------------------------------------------------------------------------
DEMO_RADIUS ?= 6
DEMO_DEFS = -DINCRA_DEMO=1 -DINCRA_DEMO_RADIUS=$(DEMO_RADIUS)
DEMO_DIR = $(OUTPUT_DIR)/demo
DEMO_RELEASE_DIR = $(OUTPUT_DIR)/demo-release

demo:
	mkdir -p $(DEMO_DIR)
	clang++ $(SRC) -o "$(DEMO_DIR)/$(DEMO_NAME)" $(INCLUDE_DIRS) $(ASSETS_DEF) $(DEMO_DEFS) $(RPATH) $(FRAMEWORK) $(FRAMEWORK_BIN) -std=c++20 $(OPT)

demo-release:
	mkdir -p $(DEMO_RELEASE_DIR)
	clang++ $(PACKER_SRC) -o $(DEMO_RELEASE_DIR)/AssetPacker $(INCLUDE_DIRS) -std=c++20 -O2
	$(DEMO_RELEASE_DIR)/AssetPacker assets $(DEMO_RELEASE_DIR)/AssetPack.cpp
	clang++ $(SRC) $(DEMO_RELEASE_DIR)/AssetPack.cpp -o "$(DEMO_RELEASE_DIR)/$(DEMO_NAME)" $(INCLUDE_DIRS) $(RELEASE_DEFS) $(DEMO_DEFS) $(RPATH) $(FRAMEWORK) $(FRAMEWORK_BIN) -std=c++20 $(OPT)

demo-win:
	powershell -NoProfile -Command "New-Item -ItemType Directory -Force '$(DEMO_DIR)' | Out-Null"
	$(WIN_ICON)
	g++ $(SRC) $(WIN_ICON_RES) -o "$(DEMO_DIR)/$(DEMO_NAME).exe" $(WIN_INCLUDE_DIRS) $(ASSETS_DEF) $(DEMO_DEFS) $(WIN_LIBS) -std=c++20 $(OPT)
	powershell -NoProfile -Command "Copy-Item -Force '$(SDL_DIR)/lib/x64/SDL3.dll' '$(DEMO_DIR)/'"

demo-release-win:
	powershell -NoProfile -Command "New-Item -ItemType Directory -Force '$(DEMO_RELEASE_DIR)' | Out-Null"
	g++ $(PACKER_SRC) -o $(DEMO_RELEASE_DIR)/AssetPacker.exe $(INCLUDE_DIRS) -static -std=c++20 -O2
	powershell -NoProfile -Command "& './$(DEMO_RELEASE_DIR)/AssetPacker.exe' assets '$(DEMO_RELEASE_DIR)/AssetPack.cpp'"
	$(WIN_ICON)
	g++ $(SRC) $(DEMO_RELEASE_DIR)/AssetPack.cpp $(WIN_ICON_RES) -o "$(DEMO_RELEASE_DIR)/$(DEMO_NAME).exe" $(WIN_INCLUDE_DIRS) $(RELEASE_DEFS) $(DEMO_DEFS) $(WIN_LIBS) -std=c++20 $(OPT) -mwindows
	powershell -NoProfile -Command "Copy-Item -Force '$(SDL_DIR)/lib/x64/SDL3.dll' '$(DEMO_RELEASE_DIR)/'"

.PHONY: demo demo-win demo-release demo-release-win

# ---------------------------------------------------------------------------
# Web builds (browser / itch.io HTML5), with Emscripten. Assets are packed in
# like a release build; saves go in the browser's storage (web/pre.js).
#   1. Install Emscripten once:   brew install emscripten   (or the emsdk)
#   2. make web-sdl               builds SDL3 for the web into build/web-sdl/ (once)
#   3. make demo-web              the demo:      build/demo-web/index.html (+ .js, .wasm)
#      make web                   the full game: build/web/index.html
# Try it locally: cd build/demo-web && python3 -m http.server, then open
# http://localhost:8000 (opening index.html straight from disk won't work).
# For itch.io, zip the three files in the folder (index.html at the top of the
# zip) and upload it as "This file will be played in the browser", 1280 x 720.
# ---------------------------------------------------------------------------
EMCC ?= emcc
EMCMAKE ?= emcmake
HOST_CXX ?= clang++
SDL_WEB_TAG ?= release-3.2.24
WEB_SDL_DIR = $(OUTPUT_DIR)/web-sdl
WEB_SDL = $(WEB_SDL_DIR)/install
WEB_FLAGS = -I$(WEB_SDL)/include $(WEB_SDL)/lib/libSDL3.a -std=c++20 -O2 \
	-sALLOW_MEMORY_GROWTH=1 -sSTACK_SIZE=1048576 -sFORCE_FILESYSTEM=1 -sGL_ENABLE_GET_PROC_ADDRESS -sMINIFY_HTML=0 -lidbfs.js \
	--pre-js web/pre.js --shell-file web/shell.html
WEB_DIR = $(OUTPUT_DIR)/web
DEMO_WEB_DIR = $(OUTPUT_DIR)/demo-web

web-sdl:
	mkdir -p $(WEB_SDL_DIR)
	if [ ! -d $(WEB_SDL_DIR)/src ]; then git clone --depth 1 --branch $(SDL_WEB_TAG) https://github.com/libsdl-org/SDL.git $(WEB_SDL_DIR)/src; fi
	$(EMCMAKE) cmake -S $(WEB_SDL_DIR)/src -B $(WEB_SDL_DIR)/cmake -DCMAKE_BUILD_TYPE=Release -DSDL_SHARED=OFF -DSDL_STATIC=ON -DSDL_TESTS=OFF -DSDL_EXAMPLES=OFF -DCMAKE_INSTALL_PREFIX=$(abspath $(WEB_SDL))
	cmake --build $(WEB_SDL_DIR)/cmake -j8
	cmake --install $(WEB_SDL_DIR)/cmake

web:
	mkdir -p $(WEB_DIR)
	$(HOST_CXX) $(PACKER_SRC) -o $(WEB_DIR)/AssetPacker $(INCLUDE_DIRS) -std=c++20 -O2
	$(WEB_DIR)/AssetPacker assets $(WEB_DIR)/AssetPack.cpp
	$(EMCC) $(SRC) $(WEB_DIR)/AssetPack.cpp -o $(WEB_DIR)/index.html $(INCLUDE_DIRS) $(RELEASE_DEFS) $(WEB_FLAGS)
	rm -f $(WEB_DIR)/AssetPacker $(WEB_DIR)/AssetPack.cpp

demo-web:
	mkdir -p $(DEMO_WEB_DIR)
	$(HOST_CXX) $(PACKER_SRC) -o $(DEMO_WEB_DIR)/AssetPacker $(INCLUDE_DIRS) -std=c++20 -O2
	$(DEMO_WEB_DIR)/AssetPacker assets $(DEMO_WEB_DIR)/AssetPack.cpp
	$(EMCC) $(SRC) $(DEMO_WEB_DIR)/AssetPack.cpp -o $(DEMO_WEB_DIR)/index.html $(INCLUDE_DIRS) $(RELEASE_DEFS) $(DEMO_DEFS) $(WEB_FLAGS)
	rm -f $(DEMO_WEB_DIR)/AssetPacker $(DEMO_WEB_DIR)/AssetPack.cpp

.PHONY: web-sdl web demo-web

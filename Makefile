PROJECTNAME = game
OUTPUT_DIR = build

RPATH = -rpath /Library/Frameworks
FRAMEWORK = -F/Library/Frameworks
FRAMEWORK_BIN = -framework sdl3
INCLUDE_DIRS = -Iinclude

SRC = $(wildcard src/*.cpp)

default:
	clang++ $(SRC) -o $(OUTPUT_DIR)/$(PROJECTNAME) $(INCLUDE_DIRS) $(RPATH) $(FRAMEWORK) $(FRAMEWORK_BIN) -std=c++20

# Tech tree editor: make editor, then run build/TechTreeEditor
EDITOR_SRC = $(wildcard tools/TechTreeEditor/src/*.cpp) src/TechData.cpp

editor:
	clang++ $(EDITOR_SRC) -o $(OUTPUT_DIR)/TechTreeEditor $(INCLUDE_DIRS) -Itools/TechTreeEditor/include $(RPATH) $(FRAMEWORK) $(FRAMEWORK_BIN) -std=c++20

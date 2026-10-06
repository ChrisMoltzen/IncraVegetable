PROJECTNAME = game
OUTPUT_DIR = build

RPATH = -rpath /Library/Frameworks
FRAMEWORK = -F/Library/Frameworks
FRAMEWORK_BIN = -framework sdl3
INCLUDE_DIRS = -Iinclude

SRC = $(wildcard src/*.cpp)

default:
	clang++ $(SRC) -o $(OUTPUT_DIR)/$(PROJECTNAME) $(INCLUDE_DIRS) $(RPATH) $(FRAMEWORK) $(FRAMEWORK_BIN) -std=c++20
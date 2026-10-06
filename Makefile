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
	clang++ $(EDITOR_SRC) -o $(OUTPUT_DIR)/TechTreeEditor -Itools/TechTreeEditor/include $(INCLUDE_DIRS) $(RPATH) $(FRAMEWORK) $(FRAMEWORK_BIN) -std=c++20


 


WIN_INCLUDE_DIRS = -Iinclude -IC:/SDL/include
LIB_DIRS = -LC:/SDL/lib -lC:\SDL\lib\x86\SDL3


LIBS = -lstdc++ 


win:
	g++ $(SRC) -o $(OUTPUT_DIR)/$(PROJECTNAME) $(WIN_INCLUDE_DIRS) $(LIB_DIRS) $(LIBS) -std=c++20
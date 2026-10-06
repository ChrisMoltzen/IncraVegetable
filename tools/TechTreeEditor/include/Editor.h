// Editor.h - the tech tree editor: a canvas showing the tree, a panel for
// editing the selected tech, and saving to include/TechTreeData.h.
#pragma once

#include "TechData.h"
#include "EditorUi.h"

#include <SDL3/SDL.h>
#include <string>
#include <vector>

class Editor {
public:
    bool init(int argc, char** argv);
    void shutdown();
    void event(const SDL_Event& e);
    void frame();
    bool running() const { return running_; }

    // For automated tests: run a frame with the given input.
    Input& input() { return in_; }
    SDL_Renderer* renderer() const { return r_; }

private:
    using TechDef = techdata::TechDef;

    // File
    std::string findFile(int argc, char** argv) const;
    bool load(const std::string& path);
    bool save();

    // Editing (each of these is one undo step)
    void pushUndo();
    void undo();
    void redo();
    int addTech(float gx, float gy);
    int duplicateTech(int index);
    void deleteTech(int index);
    bool renameTech(int index, const std::string& newId);
    void toggleRequirement(int tech, int needed);
    std::string uniqueId(const std::string& base) const;
    int indexOf(const std::string& id) const;
    bool spotFree(float gx, float gy, int ignore) const;
    void freeSpotNear(float& gx, float& gy, int ignore) const;

    // Canvas
    SDL_FRect canvasRect() const;
    SDL_FRect panelRect() const;
    SDL_FPoint origin() const;
    SDL_FRect nodeRect(const TechDef& t) const;
    SDL_FPoint gridAt(float x, float y) const;
    int nodeAt(float x, float y) const;
    void fitView();
    void canvasInput();
    void globalKeys();

    // Drawing
    void drawCanvas();
    void drawTopBar();
    void drawPanel();
    void drawTechPanel(float x, float& y, float w);
    void drawTreePanel(float x, float& y, float w);
    void drawModal();
    void toast(const std::string& msg, bool error = false);

    SDL_Window* win_ = nullptr;
    SDL_Renderer* r_ = nullptr;
    Input in_;
    Ui ui_;
    bool running_ = true;
    int winW_ = 1440, winH_ = 860;

    std::vector<TechDef> techs_;
    int sel_ = -1;
    std::string path_;
    bool dirty_ = false;
    std::vector<std::string> fileErrors_;

    struct Snapshot {
        std::vector<TechDef> techs;
        int sel;
    };
    std::vector<Snapshot> undo_, redo_;

    float camX_ = 0.f, camY_ = 0.f, zoom_ = 1.f;
    int drag_ = -1;
    bool dragMoved_ = false;
    float grabX_ = 0.f, grabY_ = 0.f;
    bool panning_ = false;
    bool linkMode_ = false;
    float panelScroll_ = 0.f, panelContentH_ = 0.f;

    std::string toast_;
    bool toastError_ = false;
    float toastTime_ = 0.f;

    enum class Modal { None, Quit, Reload };
    Modal modal_ = Modal::None;
};

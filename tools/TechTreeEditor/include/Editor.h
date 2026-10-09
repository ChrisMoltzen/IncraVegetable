// Editor.h - the tech tree editor: a canvas showing the tree, a panel for
// editing the selected tech, and saving to include/TechTreeData.h.
#pragma once

#include "TechData.h"
#include "EditorUi.h"

#include <SDL3/SDL.h>
#include <string>
#include <unordered_map>
#include <unordered_set>
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
    void deleteTechs(std::vector<int> indices); // several at once, one undo step
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
    // Crops: listed on the tree panel; click one to edit it in the crop panel.
    void drawCropsSection(float x, float& y, float w);
    void drawCropPanel(float x, float& y, float w);
    void drawCrop(int crop, float cx, float cy, float size); // its image if there is one, else its built-in look
    int addCrop();
    void deleteCrop(int index);
    bool renameCrop(int index, const std::string& newId);
    void makeUnlockTech(int crop);
    std::vector<std::string> techsUnlocking(int tier) const; // names of techs that raise Crops to >= tier
    std::string uniqueCropId(const std::string& base) const;
    void drawModal();
    void drawStatPicker();
    void drawIconPicker();

    // Icons: the built-in pictures (from art_templates/tree/icons/) and your
    // own (assets/tree/icons/), which win when both exist - as in the game.
    std::string projectRoot() const;
    void loadIcons();
    void drawIcon(const std::string& name, const SDL_FRect& rc, const std::string& title);
    bool hasIconImage(const std::string& name) const { return iconTex_.count(name) != 0; }
    void drawTreeTotals(float x, float& y, float w);
    void toast(const std::string& msg, bool error = false);

    SDL_Window* win_ = nullptr;
    SDL_Renderer* r_ = nullptr;
    Input in_;
    Ui ui_;
    bool running_ = true;
    int winW_ = 1440, winH_ = 860;

    std::vector<TechDef> techs_;
    int sel_ = -1;                 // the tech shown in the panel
    std::unordered_set<int> picked_; // every selected tech (sel_ among them); drag/arrows/Delete act on all
    bool isPicked(int i) const { return picked_.count(i) != 0; }
    void selectOnly(int i);        // -1 = nothing
    void syncSelection();          // keeps picked_ and sel_ consistent after other edits
    std::vector<int> pickedList() const;
    // Box selection: drag on empty canvas.
    bool boxing_ = false, boxAdds_ = false;
    float boxX0_ = 0.f, boxY0_ = 0.f;
    std::vector<techdata::CropDef> crops_;
    int crop_ = -1; // crop being edited (-1 = none)
    std::unordered_map<std::string, SDL_Texture*> cropTex_; // assets/crops/<id>.png
    std::string path_;
    bool dirty_ = false;
    std::vector<std::string> fileErrors_;
    // Problems with the tree, worked out once per frame (not once per tech).
    std::vector<techdata::Problem> problems_;
    std::vector<std::string> cropProblems_;
    std::unordered_set<std::string> problemIds_;
    void refreshProblems();

    struct Snapshot {
        std::vector<TechDef> techs;
        int sel;
        std::vector<techdata::CropDef> crops;
        int crop;
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

    enum class Modal { None, Quit, Reload, StatPicker, IconPicker };
    Modal modal_ = Modal::None;
    int pickTech_ = -1, pickEffect_ = -1; // the effect whose stat is being chosen
    int iconTech_ = -1;                   // the tech whose icon is being chosen
    std::unordered_map<std::string, SDL_Texture*> iconTex_;
    std::vector<std::string> customIcons_; // names of images in assets/tree/icons/ that aren't built-in icons
};

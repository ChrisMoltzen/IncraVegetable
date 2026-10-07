#include "Editor.h"

#include "CropLooks.h"

#include <algorithm>
#include <cctype>
#include <cmath>

using namespace gfx;
using techdata::Effect;
using techdata::Op;
using techdata::Requirement;

namespace {
// Same layout numbers as the game's tech tree screen (icon tiles), so it looks the same.
constexpr float kSpacingX = 140.f, kSpacingY = 112.f;
constexpr float kNodeW = 84.f, kNodeH = 84.f;
constexpr float kTopBar = 56.f;
constexpr float kPanelW = 560.f;
constexpr size_t kMaxUndo = 300;

float snapHalf(float v) { return std::round(v * 2.f) / 2.f; }
} // namespace

// ===========================================================================
// Start-up and files
// ===========================================================================

bool Editor::init(int argc, char** argv) {
    if (!SDL_Init(SDL_INIT_VIDEO)) {
        SDL_Log("SDL_Init failed: %s", SDL_GetError());
        return false;
    }
    SDL_Rect usable{0, 0, 1600, 940};
    if (SDL_DisplayID d = SDL_GetPrimaryDisplay()) SDL_GetDisplayUsableBounds(d, &usable);
    winW_ = std::clamp(static_cast<int>(usable.w * 0.92f), 1100, 1600);
    winH_ = std::clamp(static_cast<int>(usable.h * 0.9f), 680, 960);
    if (!SDL_CreateWindowAndRenderer("Tech Tree Editor - IncraVegetable", winW_, winH_, SDL_WINDOW_RESIZABLE, &win_, &r_)) {
        SDL_Log("Couldn't create window: %s", SDL_GetError());
        return false;
    }
    SDL_SetWindowMinimumSize(win_, 1100, 680);
    SDL_SetRenderDrawBlendMode(r_, SDL_BLENDMODE_BLEND);
    SDL_SetRenderVSync(r_, 1);

    path_ = findFile(argc, argv);
    if (!load(path_)) {
        toast("No tech tree found at " + path_ + " - Save will create it", true);
    }
    loadIcons();
    return true;
}

void Editor::shutdown() {
    for (auto& [name, tex] : iconTex_) SDL_DestroyTexture(tex);
    iconTex_.clear();
    for (auto& [name, tex] : cropTex_) SDL_DestroyTexture(tex);
    cropTex_.clear();
    if (r_) SDL_DestroyRenderer(r_);
    if (win_) SDL_DestroyWindow(win_);
    SDL_Quit();
}

// Looks for include/TechTreeData.h from where the editor was started, and
// from where the executable is (build/ in the project).
std::string Editor::findFile(int argc, char** argv) const {
    if (argc > 1) return argv[1];
    std::vector<std::string> bases = {""};
    if (const char* base = SDL_GetBasePath()) bases.push_back(base);
    for (const auto& b : bases) {
        for (const char* up : {"", "../", "../../", "../../../"}) {
            std::string p = b + up + "include/TechTreeData.h";
            SDL_PathInfo info;
            if (SDL_GetPathInfo(p.c_str(), &info) && info.type == SDL_PATHTYPE_FILE) return p;
        }
    }
    return "include/TechTreeData.h";
}

// The project folder: the one holding include/TechTreeData.h.
std::string Editor::projectRoot() const {
    std::string dir = path_;
    size_t slash = dir.find_last_of("/\\");
    dir = slash == std::string::npos ? std::string(".") : dir.substr(0, slash);
    if (dir.size() >= 7 && dir.compare(dir.size() - 7, 7, "include") == 0) {
        dir = dir.substr(0, dir.size() - 7);
        if (!dir.empty() && (dir.back() == '/' || dir.back() == '\\')) dir.pop_back();
        if (dir.empty()) dir = ".";
    }
    return dir;
}

void Editor::loadIcons() {
    for (auto& [name, tex] : iconTex_) SDL_DestroyTexture(tex);
    iconTex_.clear();
    for (auto& [name, tex] : cropTex_) SDL_DestroyTexture(tex);
    cropTex_.clear();
    {
        // Crop artwork, so crops (and crop icons) look as they will in the game.
        const std::string dir = projectRoot() + "/assets/crops/";
        int n = 0;
        if (char** files = SDL_GlobDirectory(dir.c_str(), "*.png", 0, &n)) {
            for (int k = 0; k < n; ++k) {
                std::string f = files[k], stem = f.substr(0, f.size() - 4);
                if (SDL_Texture* t = loadImage(r_, dir + f)) cropTex_[stem] = t;
            }
            SDL_free(files);
        }
    }
    customIcons_.clear();
    const std::string root = projectRoot();
    const std::string mine = root + "/assets/tree/icons/", templates = root + "/art_templates/tree/icons/";
    const char* exts[] = {".png", ".jpg", ".jpeg", ".bmp", ".tga"};
    // Your own images first (they're what the game shows), then the built-in pictures.
    int count = 0;
    if (char** files = SDL_GlobDirectory(mine.c_str(), "*", 0, &count)) {
        for (int k = 0; k < count; ++k) {
            std::string f = files[k];
            std::string lower = f;
            for (char& c : lower) c = static_cast<char>(std::tolower(static_cast<unsigned char>(c)));
            for (const char* e : exts) {
                size_t n = std::string(e).size();
                if (lower.size() <= n || lower.compare(lower.size() - n, n, e) != 0) continue;
                std::string name = f.substr(0, f.size() - n);
                if (!techdata::isValidId(name) || iconTex_.count(name)) break;
                if (SDL_Texture* t = loadImage(r_, mine + f)) {
                    iconTex_[name] = t;
                    if (!techdata::isBuiltinIcon(name)) customIcons_.push_back(name);
                }
                break;
            }
        }
        SDL_free(files);
    }
    for (const auto& b : techdata::builtinIcons()) {
        if (iconTex_.count(b.key)) continue;
        if (SDL_Texture* t = loadImage(r_, templates + b.key + ".png")) iconTex_[b.key] = t;
    }
    std::sort(customIcons_.begin(), customIcons_.end());
}

// The icon if there's an image for it; otherwise its initials on a badge (as in the game).
void Editor::drawIcon(const std::string& name, const SDL_FRect& rc, const std::string& title) {
    auto it = iconTex_.find(name);
    if (it != iconTex_.end()) {
        SDL_RenderTexture(r_, it->second, nullptr, &rc);
        return;
    }
    if (name.rfind("crop_", 0) == 0) { // a crop used as an icon
        for (int c = 0; c < static_cast<int>(crops_.size()); ++c)
            if (crops_[c].id == name.substr(5)) return drawCrop(c, rc.x + rc.w / 2, rc.y + rc.h / 2, rc.w * 0.88f);
    }
    fillRound(r_, {rc.x + rc.w * 0.04f, rc.y + rc.h * 0.04f, rc.w * 0.92f, rc.h * 0.92f}, rc.w * 0.2f, SDL_Color{70, 110, 160, 255});
    std::string initials;
    bool next = true;
    for (char c : title.empty() ? name : title) {
        if (next && std::isalpha(static_cast<unsigned char>(c))) initials += static_cast<char>(std::toupper(static_cast<unsigned char>(c)));
        next = c == ' ' || c == '_';
    }
    initials = initials.substr(0, 2);
    float sc = std::max(1.f, rc.w / 32.f);
    text(r_, rc.x + rc.w / 2, rc.y + rc.h / 2 - 4 * sc, initials, sc, col::text, Align::Center);
}

bool Editor::load(const std::string& path) {
    size_t size = 0;
    void* data = SDL_LoadFile(path.c_str(), &size);
    if (!data) return false;
    std::string header(static_cast<const char*>(data), size);
    SDL_free(data);

    std::string text;
    if (!techdata::fromHeader(header, text)) {
        fileErrors_ = {"The file doesn't contain tech tree data (no R\"TECHTREE(...)\" section)."};
        toast("Couldn't read the tech tree from " + path, true);
        return false;
    }
    techdata::ParseResult res = techdata::parse(text);
    techs_ = std::move(res.techs);
    crops_ = res.crops.empty() ? techdata::defaultCrops() : std::move(res.crops);
    techdata::setCrops(crops_);
    fileErrors_ = std::move(res.errors);
    sel_ = -1;
    crop_ = -1;
    undo_.clear();
    redo_.clear();
    dirty_ = false;
    fitView();
    toast(fileErrors_.empty() ? strf("Loaded %d techs", static_cast<int>(techs_.size()))
                              : strf("Loaded with %d problem(s) in the file - see the panel", static_cast<int>(fileErrors_.size())),
          !fileErrors_.empty());
    return true;
}

bool Editor::save() {
    ui_.commitFocus();
    std::string header = techdata::toHeader(techs_, crops_);
    std::string tmp = path_ + ".tmp";
    bool ok = SDL_SaveFile(tmp.c_str(), header.data(), header.size());
    if (ok && !SDL_RenamePath(tmp.c_str(), path_.c_str())) {
        SDL_RemovePath(path_.c_str());
        ok = SDL_RenamePath(tmp.c_str(), path_.c_str());
    }
    if (!ok) {
        toast("Couldn't save " + path_ + ": " + SDL_GetError(), true);
        return false;
    }
    dirty_ = false;
    fileErrors_.clear();
    size_t problems = techdata::validate(techs_).size() + techdata::validateCrops(crops_, techs_).size();
    toast(problems ? strf("Saved - but %d problem(s) need fixing", static_cast<int>(problems))
                   : std::string("Saved. Rebuild the game to use the new tree."),
          problems != 0);
    return true;
}

// ===========================================================================
// Editing
// ===========================================================================

void Editor::pushUndo() {
    undo_.push_back({techs_, sel_, crops_, crop_});
    if (undo_.size() > kMaxUndo) undo_.erase(undo_.begin());
    redo_.clear();
    dirty_ = true;
}

void Editor::undo() {
    if (undo_.empty()) return toast("Nothing to undo");
    redo_.push_back({techs_, sel_, crops_, crop_});
    techs_ = std::move(undo_.back().techs);
    sel_ = std::min(undo_.back().sel, static_cast<int>(techs_.size()) - 1);
    crops_ = std::move(undo_.back().crops);
    crop_ = std::min(undo_.back().crop, static_cast<int>(crops_.size()) - 1);
    undo_.pop_back();
    dirty_ = true;
}

void Editor::redo() {
    if (redo_.empty()) return toast("Nothing to redo");
    undo_.push_back({techs_, sel_, crops_, crop_});
    techs_ = std::move(redo_.back().techs);
    sel_ = std::min(redo_.back().sel, static_cast<int>(techs_.size()) - 1);
    crops_ = std::move(redo_.back().crops);
    crop_ = std::min(redo_.back().crop, static_cast<int>(crops_.size()) - 1);
    redo_.pop_back();
    dirty_ = true;
}

int Editor::indexOf(const std::string& id) const {
    for (int i = 0; i < static_cast<int>(techs_.size()); ++i)
        if (techs_[i].id == id) return i;
    return -1;
}

std::string Editor::uniqueId(const std::string& base) const {
    if (indexOf(base) < 0) return base;
    for (int n = 2;; ++n) {
        std::string id = base + "_" + std::to_string(n);
        if (indexOf(id) < 0) return id;
    }
}

bool Editor::spotFree(float gx, float gy, int ignore) const {
    for (int i = 0; i < static_cast<int>(techs_.size()); ++i) {
        if (i == ignore) continue;
        if (std::fabs(techs_[i].gridX - gx) < 0.75f && std::fabs(techs_[i].gridY - gy) < 0.75f) return false;
    }
    return true;
}

void Editor::freeSpotNear(float& gx, float& gy, int ignore) const {
    for (int ring = 0; ring < 20; ++ring) {
        for (int dy = -ring; dy <= ring; ++dy)
            for (int dx = -ring; dx <= ring; ++dx) {
                if (std::max(std::abs(dx), std::abs(dy)) != ring) continue;
                if (spotFree(gx + dx, gy + dy, ignore)) {
                    gx += dx;
                    gy += dy;
                    return;
                }
            }
    }
}

int Editor::addTech(float gx, float gy) {
    pushUndo();
    TechDef t;
    t.id = uniqueId("new_tech");
    t.name = "New Tech";
    t.description = "Describe what this upgrade does.";
    t.maxLevel = 5;
    t.baseCost = 100;
    t.costGrowth = 1.8;
    t.gridX = snapHalf(gx);
    t.gridY = snapHalf(gy);
    t.effects.push_back(Effect{"dayLength", Op::Add, 2.f});
    // Handy default: needs whatever was selected.
    if (sel_ >= 0 && sel_ < static_cast<int>(techs_.size())) t.needs.push_back({techs_[sel_].id, 1});
    techs_.push_back(t);
    sel_ = static_cast<int>(techs_.size()) - 1;
    panelScroll_ = 0;
    toast("Added a new tech - give it an id, name and effect in the panel");
    return sel_;
}

int Editor::duplicateTech(int index) {
    if (index < 0) return -1;
    pushUndo();
    TechDef t = techs_[index];
    t.id = uniqueId(t.id + "_copy");
    t.name += " (copy)";
    freeSpotNear(t.gridX, t.gridY, -1);
    techs_.push_back(t);
    sel_ = static_cast<int>(techs_.size()) - 1;
    toast("Duplicated");
    return sel_;
}

void Editor::deleteTech(int index) {
    if (index < 0) return;
    pushUndo();
    std::string id = techs_[index].id;
    techs_.erase(techs_.begin() + index);
    int refs = 0;
    for (auto& t : techs_) {
        size_t before = t.needs.size();
        std::erase_if(t.needs, [&](const Requirement& q) { return q.id == id; });
        refs += static_cast<int>(before - t.needs.size());
    }
    sel_ = -1;
    toast(refs ? strf("Deleted '%s' and removed it from %d requirement(s). Ctrl+Z to undo", id.c_str(), refs)
               : strf("Deleted '%s'. Ctrl+Z to undo", id.c_str()));
}

bool Editor::renameTech(int index, const std::string& newIdRaw) {
    std::string newId = newIdRaw;
    newId.erase(std::remove(newId.begin(), newId.end(), ' '), newId.end());
    std::string oldId = techs_[index].id;
    if (newId == oldId) return true;
    if (!techdata::isValidId(newId)) {
        toast("An id can only use letters, numbers and _", true);
        return false;
    }
    if (indexOf(newId) >= 0) {
        toast("Another tech already uses the id '" + newId + "'", true);
        return false;
    }
    pushUndo();
    techs_[index].id = newId;
    for (auto& t : techs_)
        for (auto& q : t.needs)
            if (q.id == oldId) q.id = newId;
    toast("Renamed id. Note: old saves keep their levels under the old id.");
    return true;
}

void Editor::toggleRequirement(int tech, int needed) {
    if (tech < 0 || needed < 0 || tech == needed) return;
    pushUndo();
    auto& needs = techs_[tech].needs;
    const std::string& id = techs_[needed].id;
    auto it = std::find_if(needs.begin(), needs.end(), [&](const Requirement& q) { return q.id == id; });
    if (it != needs.end()) {
        needs.erase(it);
        toast(techs_[tech].name + " no longer needs " + techs_[needed].name);
    } else {
        needs.push_back({id, 1});
        toast(techs_[tech].name + " now needs " + techs_[needed].name + " (level 1)");
    }
}

// ===========================================================================
// Canvas geometry
// ===========================================================================

SDL_FRect Editor::canvasRect() const { return {0, kTopBar, winW_ - kPanelW, winH_ - kTopBar}; }
SDL_FRect Editor::panelRect() const { return {winW_ - kPanelW, kTopBar, kPanelW, winH_ - kTopBar}; }

SDL_FPoint Editor::origin() const {
    SDL_FRect c = canvasRect();
    return {c.x + c.w * 0.5f + camX_, c.y + 110.f + camY_};
}

SDL_FRect Editor::nodeRect(const TechDef& t) const {
    SDL_FPoint o = origin();
    float cx = o.x + t.gridX * kSpacingX * zoom_, cy = o.y + t.gridY * kSpacingY * zoom_;
    return {cx - kNodeW * zoom_ * 0.5f, cy - kNodeH * zoom_ * 0.5f, kNodeW * zoom_, kNodeH * zoom_};
}

SDL_FPoint Editor::gridAt(float x, float y) const {
    SDL_FPoint o = origin();
    return {(x - o.x) / (kSpacingX * zoom_), (y - o.y) / (kSpacingY * zoom_)};
}

int Editor::nodeAt(float x, float y) const {
    for (int i = static_cast<int>(techs_.size()) - 1; i >= 0; --i)
        if (inside(x, y, nodeRect(techs_[i]))) return i;
    return -1;
}

void Editor::fitView() {
    SDL_FRect c = canvasRect();
    if (techs_.empty()) {
        zoom_ = 1.f;
        camX_ = camY_ = 0.f;
        return;
    }
    float minX = 1e9f, maxX = -1e9f, minY = 1e9f, maxY = -1e9f;
    for (const auto& t : techs_) {
        minX = std::min(minX, t.gridX); maxX = std::max(maxX, t.gridX);
        minY = std::min(minY, t.gridY); maxY = std::max(maxY, t.gridY);
    }
    float w = (maxX - minX) * kSpacingX + kNodeW + 80.f, h = (maxY - minY) * kSpacingY + kNodeH + 120.f;
    zoom_ = std::clamp(std::min(c.w / w, c.h / h), 0.35f, 1.2f);
    float midX = (minX + maxX) * 0.5f, midY = (minY + maxY) * 0.5f;
    camX_ = -midX * kSpacingX * zoom_;
    camY_ = c.h * 0.5f - 110.f - midY * kSpacingY * zoom_;
}

// ===========================================================================
// Input
// ===========================================================================

void Editor::event(const SDL_Event& e) {
    switch (e.type) {
    case SDL_EVENT_QUIT:
        if (dirty_) modal_ = Modal::Quit;
        else running_ = false;
        break;
    case SDL_EVENT_MOUSE_MOTION:
        in_.mx = e.motion.x;
        in_.my = e.motion.y;
        in_.dx += e.motion.xrel;
        in_.dy += e.motion.yrel;
        break;
    case SDL_EVENT_MOUSE_BUTTON_DOWN:
        in_.mx = e.button.x;
        in_.my = e.button.y;
        if (e.button.button == SDL_BUTTON_LEFT) {
            in_.down = in_.pressed = true;
            in_.doubleClick = e.button.clicks >= 2;
        } else if (e.button.button == SDL_BUTTON_RIGHT) {
            in_.rightDown = true;
        } else if (e.button.button == SDL_BUTTON_MIDDLE) {
            in_.middleDown = true;
        }
        break;
    case SDL_EVENT_MOUSE_BUTTON_UP:
        if (e.button.button == SDL_BUTTON_LEFT) {
            in_.down = false;
            in_.released = true;
        } else if (e.button.button == SDL_BUTTON_RIGHT) {
            in_.rightDown = false;
        } else if (e.button.button == SDL_BUTTON_MIDDLE) {
            in_.middleDown = false;
        }
        break;
    case SDL_EVENT_MOUSE_WHEEL:
        in_.wheel += e.wheel.y;
        break;
    case SDL_EVENT_TEXT_INPUT:
        in_.typed += e.text.text;
        break;
    case SDL_EVENT_KEY_DOWN:
        in_.keys.push_back(e.key.key);
        break;
    default:
        break;
    }
}

void Editor::globalKeys() {
    bool ctrl = in_.ctrl();
    if (ctrl && in_.key(SDLK_S)) {
        save();
        return;
    }
    if (ui_.editing()) return; // keys belong to the text field
    if (ctrl && in_.key(SDLK_Z)) {
        if (in_.shift()) redo();
        else undo();
    } else if (ctrl && in_.key(SDLK_Y)) {
        redo();
    } else if (ctrl && in_.key(SDLK_N)) {
        SDL_FRect c = canvasRect();
        SDL_FPoint g = gridAt(c.x + c.w / 2, c.y + c.h / 2);
        float gx = snapHalf(g.x), gy = snapHalf(g.y);
        freeSpotNear(gx, gy, -1);
        addTech(gx, gy);
    } else if (ctrl && in_.key(SDLK_D)) {
        duplicateTech(sel_);
    } else if (in_.key(SDLK_DELETE) || in_.key(SDLK_BACKSPACE)) {
        deleteTech(sel_);
    } else if (in_.key(SDLK_ESCAPE)) {
        if (linkMode_) linkMode_ = false;
        else sel_ = -1;
    } else if (in_.key(SDLK_F) || in_.key(SDLK_HOME)) {
        fitView();
    } else if (sel_ >= 0) {
        float dx = in_.key(SDLK_LEFT) ? -0.5f : in_.key(SDLK_RIGHT) ? 0.5f : 0.f;
        float dy = in_.key(SDLK_UP) ? -0.5f : in_.key(SDLK_DOWN) ? 0.5f : 0.f;
        if (dx != 0.f || dy != 0.f) {
            pushUndo();
            techs_[sel_].gridX += dx;
            techs_[sel_].gridY += dy;
        }
    }
}

void Editor::canvasInput() {
    SDL_FRect c = canvasRect();
    bool over = inside(in_.mx, in_.my, c);

    // Zoom around the mouse.
    if (over && in_.wheel != 0.f) {
        SDL_FPoint before = gridAt(in_.mx, in_.my);
        zoom_ = std::clamp(zoom_ * std::pow(1.12f, in_.wheel), 0.3f, 1.8f);
        camX_ = in_.mx - (c.x + c.w * 0.5f) - before.x * kSpacingX * zoom_;
        camY_ = in_.my - (c.y + 110.f) - before.y * kSpacingY * zoom_;
    }
    // Right or middle drag pans.
    if ((in_.rightDown || in_.middleDown) && (over || panning_)) {
        camX_ += in_.dx;
        camY_ += in_.dy;
    }

    if (in_.pressed && over) {
        int hit = nodeAt(in_.mx, in_.my);
        if (linkMode_ || (in_.shift() && sel_ >= 0)) {
            if (hit >= 0 && hit != sel_) toggleRequirement(sel_, hit);
            linkMode_ = false;
        } else if (hit >= 0) {
            if (hit != sel_) panelScroll_ = 0.f;
            sel_ = hit;
            drag_ = hit;
            dragMoved_ = false;
            SDL_FPoint g = gridAt(in_.mx, in_.my);
            grabX_ = g.x - techs_[hit].gridX;
            grabY_ = g.y - techs_[hit].gridY;
        } else if (in_.doubleClick) {
            SDL_FPoint g = gridAt(in_.mx, in_.my);
            addTech(g.x, g.y);
        } else {
            sel_ = -1;
            panning_ = true;
        }
    }

    if (drag_ >= 0 && in_.down && drag_ < static_cast<int>(techs_.size())) {
        SDL_FPoint g = gridAt(in_.mx, in_.my);
        float gx = snapHalf(g.x - grabX_), gy = snapHalf(g.y - grabY_);
        TechDef& t = techs_[drag_];
        if (gx != t.gridX || gy != t.gridY) {
            if (!dragMoved_) {
                pushUndo();
                dragMoved_ = true;
            }
            techs_[drag_].gridX = gx;
            techs_[drag_].gridY = gy;
        }
    } else if (panning_ && in_.down) {
        camX_ += in_.dx;
        camY_ += in_.dy;
    }
    if (!in_.down) {
        drag_ = -1;
        panning_ = false;
    }
}

// ===========================================================================
// Frame
// ===========================================================================

void Editor::frame() {
    techdata::setCrops(crops_); // descriptions of the Crops stat use the crops as they are now
    const Modal modalAtStart = modal_;
    SDL_GetWindowSize(win_, &winW_, &winH_);
    in_.mod = SDL_GetModState();
    ui_.beginFrame(in_, r_, win_);
    toastTime_ = std::max(0.f, toastTime_ - 1.f / 60.f);

    if (modal_ == Modal::None) {
        globalKeys();
        canvasInput();
    }

    SDL_SetRenderDrawColor(r_, col::bg.r, col::bg.g, col::bg.b, 255);
    SDL_RenderClear(r_);
    ui_.setBlocked(modal_ != Modal::None); // a pop-up box takes all the clicks
    drawCanvas();
    drawPanel();
    drawTopBar();
    ui_.setBlocked(false);
    if (!ui_.lastError.empty()) toast(ui_.lastError, true);
    if (toastTime_ > 0.f) {
        SDL_FRect c = canvasRect();
        Uint8 a = static_cast<Uint8>(255 * std::min(1.f, toastTime_ * 2.f));
        float w = std::min(c.w - 40.f, textWidth(toast_, 1.75f) + 32.f);
        SDL_FRect box{c.x + (c.w - w) / 2, c.y + 14, w, 36};
        fillRound(r_, box, 8, toastError_ ? SDL_Color{110, 40, 36, static_cast<Uint8>(a * 0.95f)} : SDL_Color{20, 60, 40, static_cast<Uint8>(a * 0.95f)});
        text(r_, box.x + w / 2, box.y + 11, fit(toast_, 1.75f, w - 20), 1.75f, alpha(col::text, a), Align::Center);
    }

    if (modal_ != Modal::None) {
        // A pop-up opened by this frame's click mustn't also take that click
        // (it would pick whatever option is under the pointer and close).
        ui_.setBlocked(modal_ != modalAtStart);
        drawModal();
        ui_.setBlocked(false);
    }

    ui_.endFrame();
    in_.clearFrame();
    SDL_RenderPresent(r_);
}

void Editor::toast(const std::string& msg, bool error) {
    toast_ = msg;
    toastError_ = error;
    toastTime_ = error ? 5.f : 3.f;
}

// ===========================================================================
// Drawing: canvas
// ===========================================================================

void Editor::drawCanvas() {
    SDL_FRect c = canvasRect();
    SDL_Rect clip{static_cast<int>(c.x), static_cast<int>(c.y), static_cast<int>(c.w), static_cast<int>(c.h)};
    SDL_SetRenderClipRect(r_, &clip);

    // Grid: faint lines every half step, stronger every whole step.
    SDL_FPoint o = origin();
    float sx = kSpacingX * zoom_ * 0.5f, sy = kSpacingY * zoom_ * 0.5f;
    int startX = static_cast<int>(std::floor((c.x - o.x) / sx)), endX = static_cast<int>(std::ceil((c.x + c.w - o.x) / sx));
    int startY = static_cast<int>(std::floor((c.y - o.y) / sy)), endY = static_cast<int>(std::ceil((c.y + c.h - o.y) / sy));
    for (int i = startX; i <= endX; ++i)
        fillRect(r_, {o.x + i * sx, c.y, 1, c.h}, i % 2 ? col::grid : col::gridMajor);
    for (int j = startY; j <= endY; ++j)
        fillRect(r_, {c.x, o.y + j * sy, c.w, 1}, j % 2 ? col::grid : col::gridMajor);
    text(r_, o.x + 4, o.y - 14, "0,0", 1.f, col::faint);

    std::vector<std::string> allProblems;
    std::vector<bool> hasProblem(techs_.size(), false);
    for (int i = 0; i < static_cast<int>(techs_.size()); ++i)
        hasProblem[i] = !techdata::validate(techs_, techs_[i].id).empty() && !techs_[i].id.empty();

    // Requirement lines, with an arrow pointing at the tech that needs it.
    for (int i = 0; i < static_cast<int>(techs_.size()); ++i) {
        const TechDef& t = techs_[i];
        SDL_FRect to = nodeRect(t);
        for (const auto& q : t.needs) {
            int j = indexOf(q.id);
            if (j < 0) continue;
            SDL_FRect from = nodeRect(techs_[j]);
            float x1 = from.x + from.w / 2, y1 = from.y + from.h / 2, x2 = to.x + to.w / 2, y2 = to.y + to.h / 2;
            bool related = i == sel_ || j == sel_;
            SDL_Color lc = related ? col::accent : SDL_Color{95, 140, 100, 255};
            line(r_, x1, y1, x2, y2, std::max(2.f, 4.f * zoom_), lc);
            // Arrowhead at the edge of the target box.
            float dx = x2 - x1, dy = y2 - y1, len = std::sqrt(dx * dx + dy * dy);
            if (len > 1.f) {
                dx /= len;
                dy /= len;
                float tx = std::fabs(dx) > 1e-4f ? (to.w / 2) / std::fabs(dx) : 1e9f;
                float ty = std::fabs(dy) > 1e-4f ? (to.h / 2) / std::fabs(dy) : 1e9f;
                float back = std::min(tx, ty) + 4.f;
                float ax = x2 - dx * back, ay = y2 - dy * back, s = 12.f * zoom_ + 4.f;
                triangle(r_, {ax, ay}, {ax - dx * s - dy * s * 0.6f, ay - dy * s + dx * s * 0.6f},
                         {ax - dx * s + dy * s * 0.6f, ay - dy * s - dx * s * 0.6f}, lc);
            }
            std::string lbl = strf("Lv%d", q.level);
            float mx = (x1 + x2) / 2, my = (y1 + y2) / 2, ls = std::max(1.f, 1.25f * zoom_);
            fillRound(r_, {mx - textWidth(lbl, ls) / 2 - 6, my - 6 * ls, textWidth(lbl, ls) + 12, 12 * ls}, 6, SDL_Color{40, 46, 50, 240});
            text(r_, mx, my - 4 * ls, lbl, ls, related ? col::accent : col::dim, Align::Center);
        }
    }

    // Tech boxes.
    for (int i = 0; i < static_cast<int>(techs_.size()); ++i) {
        const TechDef& t = techs_[i];
        SDL_FRect rc = nodeRect(t);
        bool selected = i == sel_;
        bool hover = inside(in_.mx, in_.my, rc) && inside(in_.mx, in_.my, c);
        SDL_Color border = selected ? col::accent : hasProblem[i] ? col::bad : hover ? SDL_Color{180, 186, 190, 255} : SDL_Color{110, 120, 128, 255};
        SDL_Color fill = t.needs.empty() ? SDL_Color{34, 62, 40, 255} : SDL_Color{44, 50, 56, 255};
        fillRound(r_, {rc.x + 3, rc.y + 5, rc.w, rc.h}, 12 * zoom_, SDL_Color{0, 0, 0, 90});
        float b = selected ? 4.f : 3.f;
        fillRound(r_, {rc.x - b, rc.y - b, rc.w + 2 * b, rc.h + 2 * b}, 14 * zoom_, border);
        fillRound(r_, rc, 11 * zoom_, fill);
        // The icon, as in the game; name (and id) underneath.
        float is = 60.f * zoom_;
        drawIcon(techdata::iconOf(t), {rc.x + (rc.w - is) / 2, rc.y + (rc.h - is) / 2, is, is}, t.name);

        std::string name = t.name.empty() ? "(no name)" : t.name;
        float labelW = rc.w * 1.25f; // neighbours can be under a column apart, so keep names short
        float ns = std::clamp(labelW / std::max(1.f, textWidth(name, 1.f)), 0.75f, 1.5f * zoom_);
        text(r_, rc.x + rc.w / 2, rc.y + rc.h + 6, fit(name, ns, labelW), ns, selected ? col::accent : col::text, Align::Center);
        if (zoom_ >= 0.8f) text(r_, rc.x + rc.w / 2, rc.y + rc.h + 8 + 8 * ns, fit(t.id, 1.f, labelW), 1.f, col::faint, Align::Center);
    }

    if (techs_.empty()) {
        text(r_, c.x + c.w / 2, c.y + c.h / 2 - 10, "No techs yet - double-click to add one", 2.f, col::dim, Align::Center);
    }

    // Hints.
    if (linkMode_ && sel_ >= 0) {
        text(r_, in_.mx + 16, in_.my + 10, "click the tech it needs", 1.5f, col::accent);
    }
    std::string help = strf("Zoom %d%%   Double-click: new tech   Drag: move   Shift+click: add/remove requirement   "
                            "Right-drag or drag empty space: pan   Wheel: zoom   F: fit",
                            static_cast<int>(zoom_ * 100));
    fillRect(r_, {c.x, c.y + c.h - 26, c.w, 26}, SDL_Color{18, 21, 20, 230});
    text(r_, c.x + 10, c.y + c.h - 18, fit(help, 1.25f, c.w - 20), 1.25f, col::dim);

    SDL_SetRenderClipRect(r_, nullptr);
}

// ===========================================================================
// Drawing: top bar
// ===========================================================================

void Editor::drawTopBar() {
    fillRect(r_, {0, 0, static_cast<float>(winW_), kTopBar}, SDL_Color{16, 19, 18, 255});
    fillRect(r_, {0, kTopBar - 1, static_cast<float>(winW_), 1}, col::panelLine);
    text(r_, 16, 12, "Tech Tree Editor", 2.f, col::accent);
    std::string file = path_;
    if (file.size() > 60) file = "..." + file.substr(file.size() - 57);
    text(r_, 16, 34, file + (dirty_ ? "   * unsaved changes" : "   saved"), 1.25f, dirty_ ? col::gold : col::dim);

    float bw = 112, bh = 36, x = winW_ - 16 - bw, y = 10;
    const float ts = 1.5f;
    if (ui_.button("top_save", {x, y, bw, bh}, "Save", true, dirty_, ts)) save();
    x -= bw + 8;
    if (ui_.button("top_reload", {x, y, bw, bh}, "Reload", true, false, ts)) {
        if (dirty_) modal_ = Modal::Reload;
        else load(path_);
    }
    x -= bw + 8;
    if (ui_.button("top_redo", {x, y, bw, bh}, "Redo", !redo_.empty(), false, ts)) redo();
    x -= bw + 8;
    if (ui_.button("top_undo", {x, y, bw, bh}, "Undo", !undo_.empty(), false, ts)) undo();
    x -= bw + 8;
    if (ui_.button("top_fit", {x, y, bw, bh}, "Fit view", true, false, ts)) fitView();
    x -= bw + 8;
    if (ui_.button("top_new", {x, y, bw, bh}, "New tech", true, false, ts)) {
        SDL_FRect c = canvasRect();
        SDL_FPoint g = gridAt(c.x + c.w / 2, c.y + c.h / 2);
        float gx = snapHalf(g.x), gy = snapHalf(g.y);
        freeSpotNear(gx, gy, -1);
        addTech(gx, gy);
    }
}

// ===========================================================================
// Drawing: side panel
// ===========================================================================

void Editor::drawPanel() {
    SDL_FRect p = panelRect();
    fillRect(r_, p, col::panel);
    fillRect(r_, {p.x, p.y, 1, p.h}, col::panelLine);

    if (inside(in_.mx, in_.my, p) && in_.wheel != 0.f) panelScroll_ -= in_.wheel * 40.f;
    panelScroll_ = std::clamp(panelScroll_, 0.f, std::max(0.f, panelContentH_ - p.h + 40.f));

    ui_.setClip(&p);
    float x = p.x + 20, w = p.w - 40, y = p.y + 18 - panelScroll_, top = y;
    if (sel_ >= 0 && sel_ < static_cast<int>(techs_.size())) {
        crop_ = -1; // selecting a tech closes the crop panel
        drawTechPanel(x, y, w);
    } else if (crop_ >= 0 && crop_ < static_cast<int>(crops_.size())) {
        drawCropPanel(x, y, w);
    } else {
        drawTreePanel(x, y, w);
    }
    panelContentH_ = y - top;
    ui_.setClip(nullptr);

    // Scroll bar.
    if (panelContentH_ > p.h) {
        float frac = p.h / panelContentH_;
        float barH = std::max(30.f, p.h * frac);
        float barY = p.y + (p.h - barH) * (panelScroll_ / std::max(1.f, panelContentH_ - p.h + 40.f));
        fillRound(r_, {p.x + p.w - 8, barY, 5, barH}, 2, SDL_Color{90, 96, 100, 200});
    }
}

namespace {
void heading(SDL_Renderer* r, float x, float& y, float w, const std::string& s) {
    y += 10;
    text(r, x, y, s, 1.75f, col::accent);
    fillRect(r, {x, y + 20, w, 1}, col::panelLine);
    y += 30;
}
} // namespace

void Editor::drawTechPanel(float x, float& y, float w) {
    const int i = sel_;
    // Lambdas below capture the index, not a reference, because the list of
    // techs can change (undo, delete) between drawing and committing.
    auto T = [this, i]() -> TechDef& { return techs_[i]; };
    TechDef& t = T();
    const float lw = 176.f, fh = 38.f, fx = x + lw, fw = w - lw;

    text(r_, x, y, "TECH", 1.5f, col::dim);
    if (ui_.button("p_delete", {x + w - 110, y - 6, 110, 32}, "Delete", true, false, 1.5f)) {
        deleteTech(i);
        return;
    }
    if (ui_.button("p_dup", {x + w - 238, y - 6, 120, 32}, "Duplicate", true, false, 1.5f)) {
        duplicateTech(i);
        return;
    }
    y += 34;

    auto label = [&](const char* s) { text(r_, x, y + (fh - 12) / 2, s, 1.5f, col::dim); };

    label("ID");
    ui_.textField("f_id", {fx, y, fw, fh}, t.id, [this, i](const std::string& v) { renameTech(i, v); });
    y += fh + 10;

    label("Name");
    ui_.textField("f_name", {fx, y, fw, fh}, t.name, [this, T](const std::string& v) {
        pushUndo();
        T().name = v;
    });
    y += fh + 10;

    // Icon: a preview, its name, and a button to pick another.
    label("Icon");
    {
        const std::string icon = techdata::iconOf(t);
        fillRound(r_, {fx, y - 6, 52, 52}, 8, SDL_Color{44, 50, 56, 255});
        drawIcon(icon, {fx + 4, y - 2, 44, 44}, t.name);
        std::string what = t.icon.empty() ? icon + "  (default)" : icon;
        text(r_, fx + 64, y + 4, fit(what, 1.5f, fw - 64 - 140), 1.5f, col::text);
        const char* where = hasIconImage(icon) ? (techdata::isBuiltinIcon(icon) ? "built-in" : "your image")
                                               : "no image: initials";
        text(r_, fx + 64, y + 24, fit(where, 1.25f, fw - 64 - 140), 1.25f, hasIconImage(icon) ? col::faint : col::accent);
        if (ui_.button("f_icon", {fx + fw - 130, y, 130, fh}, "Change...", true, false, 1.5f)) {
            ui_.commitFocus();
            loadIcons(); // pick up images added since the editor started
            iconTech_ = i;
            modal_ = Modal::IconPicker;
            return;
        }
    }
    y += fh + 16;

    label("Description");
    ui_.textField("f_desc", {fx, y, fw, 112}, t.description,
                  [this, T](const std::string& v) {
                      pushUndo();
                      T().description = v;
                  },
                  true, "Shown in the tech's tooltip");
    y += 112 + 10;

    label("Max level");
    ui_.numberField("f_max", {fx, y, 120, fh}, t.maxLevel, [this, T](double v) {
        pushUndo();
        T().maxLevel = std::max(1, static_cast<int>(v));
    }, true);
    y += fh + 10;

    label("Cost (level 1)");
    ui_.numberField("f_cost", {fx, y, 160, fh}, t.baseCost, [this, T](double v) {
        pushUndo();
        T().baseCost = std::max(0.0, v);
    });
    y += fh + 10;

    label("Cost growth");
    ui_.numberField("f_growth", {fx, y, 120, fh}, t.costGrowth, [this, T](double v) {
        if (v <= 0) return toast("Cost growth must be more than 0", true);
        pushUndo();
        T().costGrowth = v;
    });
    text(r_, fx + 132, y + 13, "x per level", 1.5f, col::faint);
    y += fh + 10;

    label("Position");
    ui_.numberField("f_px", {fx, y, 110, fh}, t.gridX, [this, T](double v) {
        pushUndo();
        T().gridX = static_cast<float>(v);
    });
    ui_.numberField("f_py", {fx + 120, y, 110, fh}, t.gridY, [this, T](double v) {
        pushUndo();
        T().gridY = static_cast<float>(v);
    });
    text(r_, fx + 242, y + 13, "col, row", 1.5f, col::faint);
    y += fh + 6;

    // ---- Requirements ----
    heading(r_, x, y, w, "Requires");
    if (ui_.button("p_addreq", {x + w - 230, y - 40, 230, 30}, linkMode_ ? "Now click a tech" : "+ Requirement", true, linkMode_, 1.5f))
        linkMode_ = !linkMode_;
    if (t.needs.empty()) {
        text(r_, x, y + 4, "Nothing - available from the start.", 1.5f, col::faint);
        y += 30;
    }
    for (int k = 0; k < static_cast<int>(t.needs.size()); ++k) {
        const Requirement& q = t.needs[k];
        int j = indexOf(q.id);
        std::string nm = j >= 0 ? techs_[j].name : q.id + " (missing!)";
        if (ui_.button("rq_go" + std::to_string(k), {x, y, w - 210, fh}, nm, true, false, 1.75f) && j >= 0) {
            sel_ = j;
            panelScroll_ = 0;
            return;
        }
        text(r_, x + w - 200, y + 12, "level", 1.5f, col::dim);
        ui_.numberField("rq_lv" + std::to_string(k), {x + w - 128, y, 76, fh}, q.level, [this, T, k](double v) {
            if (k >= static_cast<int>(T().needs.size())) return;
            pushUndo();
            T().needs[k].level = std::max(1, static_cast<int>(v));
        }, true);
        if (ui_.button("rq_x" + std::to_string(k), {x + w - 44, y, 44, fh}, "X")) {
            pushUndo();
            T().needs.erase(T().needs.begin() + k);
            return;
        }
        y += fh + 8;
    }

    // ---- Effects ----
    heading(r_, x, y, w, "Effects");
    if (ui_.button("p_addfx", {x + w - 170, y - 40, 170, 30}, "+ Add effect", true, false, 1.5f)) {
        pushUndo();
        T().effects.push_back(Effect{"dayLength", Op::Add, 1.f});
    }
    if (t.effects.empty()) {
        text(r_, x, y + 4, "None - buying this does nothing yet.", 1.5f, col::bad);
        y += 30;
    }
    const auto& stats = techdata::stats();
    for (int k = 0; k < static_cast<int>(t.effects.size()); ++k) {
        const Effect& e = t.effects[k];
        int si = techdata::statIndex(e.stat);
        std::string statName = si >= 0 ? stats[si].label : e.stat + "?";
        // Click the stat to pick from a list; click the operation to cycle (Shift+click goes backwards).
        const float sw = 196.f; // stat button width
        bool openPicker = ui_.button("fx_stat" + std::to_string(k), {x, y, sw - 18, fh}, statName, true, false, 1.5f);
        openPicker |= ui_.button("fx_statv" + std::to_string(k), {x + sw - 22, y, 22, fh}, "", true, false, 1.5f);
        triangle(r_, {x + sw - 17, y + fh / 2 - 3}, {x + sw - 5, y + fh / 2 - 3}, {x + sw - 11, y + fh / 2 + 4}, col::dim);
        if (openPicker) {
            ui_.commitFocus();
            pickTech_ = i;
            pickEffect_ = k;
            modal_ = Modal::StatPicker;
            return;
        }
        if (ui_.button("fx_op" + std::to_string(k), {x + sw + 6, y, 160, fh}, techdata::opLabel(e.op), true, false, 1.5f)) {
            pushUndo();
            int n = static_cast<int>(Op::Count);
            T().effects[k].op = static_cast<Op>((static_cast<int>(e.op) + (in_.shift() ? n - 1 : 1)) % n);
            return;
        }
        ui_.numberField("fx_amt" + std::to_string(k), {x + sw + 174, y, w - (sw + 174) - 52, fh}, e.amount, [this, T, k](double v) {
            if (k >= static_cast<int>(T().effects.size())) return;
            pushUndo();
            T().effects[k].amount = static_cast<float>(v);
        });
        if (ui_.button("fx_x" + std::to_string(k), {x + w - 44, y, 44, fh}, "X")) {
            pushUndo();
            T().effects.erase(T().effects.begin() + k);
            return;
        }
        y += fh + 4;
        // What this effect does, in words.
        TechDef one;
        one.effects = {e};
        std::string what = si >= 0 ? strf("Level 1: %s     Level %d: %s", techdata::describe(one, 1).c_str(), t.maxLevel,
                                          techdata::describe(one, t.maxLevel).c_str())
                                   : std::string("Unknown stat - pick one by clicking the first button");
        text(r_, x + 4, y + 2, fit(what, 1.25f, w), 1.25f, col::faint);
        y += 22;
        if (si >= 0) {
            text(r_, x + 4, y, fit(stats[si].help, 1.25f, w), 1.25f, SDL_Color{90, 96, 100, 255});
            y += 20;
        }
        y += 6;
    }

    // ---- Preview ----
    heading(r_, x, y, w, "Preview");
    std::string costs;
    double total = 0;
    for (int lv = 0; lv < t.maxLevel; ++lv) {
        double c = techdata::costAt(t, lv);
        total += c;
        if (lv < 12) costs += (costs.empty() ? "" : "  ") + number(c);
    }
    if (t.maxLevel > 12) costs += "  ...";
    text(r_, x, y, "Cost of each level:", 1.5f, col::dim);
    y += 22;
    for (const auto& ln : wrap(costs, static_cast<size_t>(w / 12))) {
        text(r_, x, y, ln, 1.5f, col::gold);
        y += 20;
    }
    text(r_, x, y, "All levels: " + number(total) + " coins", 1.5f, col::dim);
    y += 30;
    std::vector<int> levels = {0};
    for (int lv = 1; lv <= t.maxLevel && levels.size() < 6; ++lv) levels.push_back(lv);
    if (levels.back() != t.maxLevel) levels.push_back(t.maxLevel);
    for (size_t k = 0; k < levels.size(); ++k) {
        int lv = levels[k];
        if (k > 0 && lv != levels[k - 1] + 1) {
            text(r_, x, y, "...", 1.5f, col::faint);
            y += 20;
        }
        std::string line = strf("Level %d: ", lv) + techdata::describe(t, lv);
        text(r_, x, y, fit(line, 1.5f, w), 1.5f, lv == 0 ? col::faint : col::text);
        y += 22;
    }

    // ---- Problems ----
    auto problems = techdata::validate(techs_, t.id);
    heading(r_, x, y, w, "Problems");
    if (problems.empty()) {
        text(r_, x, y, "None", 1.5f, col::good);
        y += 26;
    }
    for (const auto& pr : problems) {
        for (const auto& ln : wrap(pr, static_cast<size_t>(w / 12))) {
            text(r_, x, y, ln, 1.5f, col::bad);
            y += 20;
        }
        y += 4;
    }
    y += 20;
}

// ===========================================================================
// Crops
// ===========================================================================

std::string Editor::uniqueCropId(const std::string& base) const {
    auto taken = [this](const std::string& id) {
        for (const auto& c : crops_)
            if (c.id == id) return true;
        return false;
    };
    if (!taken(base)) return base;
    for (int n = 2;; ++n)
        if (!taken(base + std::to_string(n))) return base + std::to_string(n);
}

int Editor::addCrop() {
    pushUndo();
    techdata::CropDef c;
    double value = 1.0;
    float grow = 1.f, pick = 1.f;
    int tier = 0;
    for (const auto& o : crops_) {
        value = std::max(value, o.value);
        grow = std::max(grow, o.grow);
        pick = std::max(pick, o.pick);
        tier = std::max(tier, o.tier);
    }
    // A step up from the best crop so far: worth more, slower, unlocked later.
    c.id = uniqueCropId("crop");
    c.name = "New crop";
    c.value = std::round(value * 2.5);
    c.grow = std::round((grow + 0.8f) * 10.f) / 10.f;
    c.pick = std::round((pick + 0.3f) * 10.f) / 10.f;
    c.weight = 15.f;
    c.tier = crops_.empty() ? 0 : tier + 1;
    c.look = "round";
    crops_.push_back(c);
    crop_ = static_cast<int>(crops_.size()) - 1;
    sel_ = -1;
    panelScroll_ = 0;
    return crop_;
}

void Editor::deleteCrop(int index) {
    if (index < 0 || index >= static_cast<int>(crops_.size())) return;
    if (crops_.size() <= 1) return toast("The game needs at least one crop", true);
    pushUndo();
    std::string gone = crops_[index].name;
    crops_.erase(crops_.begin() + index);
    crop_ = -1;
    toast("Deleted crop " + gone);
}

bool Editor::renameCrop(int index, const std::string& raw) {
    std::string id;
    for (char ch : raw)
        if (!std::isspace(static_cast<unsigned char>(ch))) id += ch;
    if (index < 0 || index >= static_cast<int>(crops_.size()) || id == crops_[index].id) return false;
    if (!techdata::isValidId(id)) {
        toast("Crop ids are letters, numbers and _ only", true);
        return false;
    }
    for (const auto& c : crops_)
        if (c.id == id) {
            toast("There's already a crop called '" + id + "'", true);
            return false;
        }
    pushUndo();
    // Techs using this crop as their icon follow it.
    for (auto& t : techs_)
        if (t.icon == "crop_" + crops_[index].id) t.icon = "crop_" + id;
    crops_[index].id = id;
    return true;
}

std::vector<std::string> Editor::techsUnlocking(int tier) const {
    std::vector<std::string> names;
    for (const auto& t : techs_) {
        Stats s;
        techdata::applyEffects(t, s, std::max(1, t.maxLevel));
        if (s.cropTier >= tier && tier > 0) names.push_back(t.name.empty() ? t.id : t.name);
    }
    return names;
}

// A tech that unlocks this crop: Crops "set at least" its tier, needing the
// tech that unlocks the tier before it, with the crop as its icon.
void Editor::makeUnlockTech(int ci) {
    if (ci < 0 || ci >= static_cast<int>(crops_.size())) return;
    const techdata::CropDef c = crops_[ci];
    pushUndo();
    TechDef t;
    t.id = uniqueId(c.id);
    t.name = c.name;
    t.description = "Unlocks " + c.name + ". Sells for " + number(c.value) + " coins each.";
    t.maxLevel = 1;
    t.baseCost = std::max(10.0, std::round(c.value * 100.0));
    t.costGrowth = 1.0;
    t.icon = "crop_" + c.id;
    t.effects.push_back(Effect{"cropTier", Op::AtLeast, static_cast<float>(c.tier)});
    // Requires whichever tech unlocks the tier below (if any), and sits under it.
    float gx = 0.f, gy = 0.f;
    int prev = -1;
    for (int i = 0; i < static_cast<int>(techs_.size()); ++i) {
        Stats s;
        techdata::applyEffects(techs_[i], s, std::max(1, techs_[i].maxLevel));
        if (s.cropTier >= c.tier - 1 && s.cropTier < c.tier && c.tier > 1) prev = i;
    }
    if (prev >= 0) {
        t.needs.push_back({techs_[prev].id, 1});
        gx = techs_[prev].gridX;
        gy = techs_[prev].gridY + 1.f;
    } else {
        for (const auto& o : techs_) gy = std::max(gy, o.gridY + 1.f);
    }
    freeSpotNear(gx, gy, -1);
    t.gridX = gx;
    t.gridY = gy;
    techs_.push_back(t);
    sel_ = static_cast<int>(techs_.size()) - 1;
    crop_ = -1;
    panelScroll_ = 0;
    toast("Added the tech '" + t.name + "' - it unlocks " + c.name);
}

void Editor::drawCrop(int ci, float cx, float cy, float size) {
    if (ci < 0 || ci >= static_cast<int>(crops_.size())) return;
    const auto& c = crops_[ci];
    auto it = cropTex_.find(c.id);
    if (it != cropTex_.end()) {
        SDL_FRect rc{cx - size / 2, cy - size / 2, size, size};
        SDL_RenderTexture(r_, it->second, nullptr, &rc);
    } else {
        croplook::draw(r_, c.look, croplook::parseColor(c.color, c.look), cx, cy, size);
    }
}

void Editor::drawCropsSection(float x, float& y, float w) {
    heading(r_, x, y, w, strf("Crops (%d)", static_cast<int>(crops_.size())));
    if (ui_.button("c_add", {x + w - 150, y - 40, 150, 30}, "+ Add crop", true, false, 1.5f)) {
        addCrop();
        return;
    }
    std::vector<int> order(crops_.size());
    for (int i = 0; i < static_cast<int>(order.size()); ++i) order[i] = i;
    std::stable_sort(order.begin(), order.end(), [this](int a, int b) { return crops_[a].tier < crops_[b].tier; });
    for (int i : order) {
        const auto& c = crops_[i];
        SDL_FRect row{x, y, w, 44};
        if (ui_.button("c_row" + std::to_string(i), row, "", true, false)) {
            crop_ = i;
            sel_ = -1;
            panelScroll_ = 0;
            return;
        }
        drawCrop(i, x + 24, y + 22, 38);
        text(r_, x + 52, y + 8, fit(c.name, 1.75f, w - 60 - 200), 1.75f, col::text);
        text(r_, x + 52, y + 27, c.tier <= 0 ? std::string("from the start") : strf("Crops level %d", c.tier), 1.25f, col::dim);
        text(r_, x + w - 12, y + 14, number(c.value) + " coins", 1.5f, col::gold, Align::Right);
        y += 48;
    }
    y += 6;
}

void Editor::drawCropPanel(float x, float& y, float w) {
    const int i = crop_;
    auto C = [this, i]() -> techdata::CropDef& { return crops_[i]; };
    techdata::CropDef& c = C();
    const float lw = 176.f, fh = 38.f, fx = x + lw, fw = w - lw;

    text(r_, x, y, "CROP", 1.5f, col::dim);
    if (ui_.button("cp_back", {x + w - 360, y - 6, 110, 32}, "< Tree", true, false, 1.5f)) {
        crop_ = -1;
        return;
    }
    if (ui_.button("cp_dup", {x + w - 238, y - 6, 120, 32}, "Duplicate", true, false, 1.5f)) {
        pushUndo();
        techdata::CropDef copy = c;
        copy.id = uniqueCropId(c.id);
        copy.name = c.name + " copy";
        crops_.push_back(copy);
        crop_ = static_cast<int>(crops_.size()) - 1;
        return;
    }
    if (ui_.button("cp_del", {x + w - 110, y - 6, 110, 32}, "Delete", crops_.size() > 1, false, 1.5f)) {
        deleteCrop(i);
        return;
    }
    y += 34;

    // Preview: ripe crop and its sprout.
    fillRound(r_, {x, y, w, 130}, 10, SDL_Color{92, 66, 44, 255});
    drawCrop(i, x + 80, y + 64, 110);
    croplook::drawSprout(r_, {x + 160, y + 30, 80, 80});
    bool image = cropTex_.count(c.id) != 0;
    text(r_, x + 260, y + 30, image ? "Your image:" : "Built-in look", 1.5f, col::text);
    text(r_, x + 260, y + 52, fit(image ? "assets/crops/" + c.id + ".png" : "Draw assets/crops/" + c.id + ".png", 1.25f, w - 270),
         1.25f, col::dim);
    if (!image) text(r_, x + 260, y + 70, fit("to replace it.", 1.25f, w - 270), 1.25f, col::dim);
    y += 142;

    auto label = [&](const char* s) { text(r_, x, y + (fh - 12) / 2, s, 1.5f, col::dim); };

    label("ID");
    ui_.textField("c_id", {fx, y, fw, fh}, c.id, [this, i](const std::string& v) { renameCrop(i, v); });
    y += fh + 10;

    label("Name");
    ui_.textField("c_name", {fx, y, fw, fh}, c.name, [this, C](const std::string& v) {
        pushUndo();
        C().name = v;
    });
    y += fh + 10;

    label("Value");
    ui_.numberField("c_value", {fx, y, 140, fh}, c.value, [this, C](double v) {
        pushUndo();
        C().value = std::max(0.0, v);
    });
    text(r_, fx + 152, y + 13, "coins each", 1.5f, col::faint);
    y += fh + 10;

    label("Grow time");
    ui_.numberField("c_grow", {fx, y, 120, fh}, c.grow, [this, C](double v) {
        if (v <= 0) return toast("Grow time must be more than 0", true);
        pushUndo();
        C().grow = static_cast<float>(v);
    });
    text(r_, fx + 132, y + 13, "x lettuce", 1.5f, col::faint);
    y += fh + 10;

    label("Pick time");
    ui_.numberField("c_pick", {fx, y, 120, fh}, c.pick, [this, C](double v) {
        if (v <= 0) return toast("Pick time must be more than 0", true);
        pushUndo();
        C().pick = static_cast<float>(v);
    });
    text(r_, fx + 132, y + 13, "x lettuce", 1.5f, col::faint);
    y += fh + 10;

    label("How often");
    ui_.numberField("c_weight", {fx, y, 120, fh}, c.weight, [this, C](double v) {
        if (v <= 0) return toast("Must be more than 0, or it's never planted", true);
        pushUndo();
        C().weight = static_cast<float>(v);
    });
    {
        float total = 0.f;
        for (const auto& o : crops_)
            if (o.tier <= c.tier) total += std::max(0.f, o.weight);
        text(r_, fx + 132, y + 13, fit(strf("%.0f%% of plants once it unlocks", total > 0 ? 100.f * c.weight / total : 0.f), 1.25f, fw - 132),
             1.25f, col::faint);
    }
    y += fh + 10;

    label("Unlocks at");
    ui_.numberField("c_tier", {fx, y, 120, fh}, c.tier, [this, C](double v) {
        pushUndo();
        C().tier = std::max(0, static_cast<int>(v));
    }, true);
    text(r_, fx + 132, y + 13, "Crops level", 1.5f, col::faint);
    y += fh + 6;
    if (c.tier <= 0) {
        text(r_, fx, y, "Planted from the start.", 1.25f, col::good);
        y += 24;
    } else {
        auto who = techsUnlocking(c.tier);
        if (who.empty()) {
            text(r_, fx, y, "Nothing unlocks it yet!", 1.25f, col::bad);
            if (ui_.button("c_unlock", {fx + fw - 230, y - 6, 230, 30}, "+ Make an unlock tech", true, true, 1.25f)) {
                makeUnlockTech(i);
                return;
            }
            y += 30;
        } else {
            std::string list;
            for (const auto& n : who) list += (list.empty() ? "" : ", ") + n;
            text(r_, fx, y, fit("Unlocked by: " + list, 1.25f, fw), 1.25f, col::good);
            y += 24;
        }
    }
    y += 6;

    // Look: one button per built-in drawing, showing it in this crop's colour.
    label("Look");
    {
        const auto& looks = croplook::looks();
        float bw = std::min(90.f, (fw - 8.f * (looks.size() - 1)) / looks.size());
        for (size_t k = 0; k < looks.size(); ++k) {
            SDL_FRect b{fx + k * (bw + 8), y - 6, bw, 74};
            bool cur = c.look == looks[k];
            if (ui_.button("c_look" + std::to_string(k), b, "", true, cur)) {
                if (!cur) {
                    pushUndo();
                    // Keep a colour the user picked; otherwise follow the new look's usual colour.
                    C().look = looks[k];
                }
            }
            croplook::draw(r_, looks[k], croplook::parseColor(c.color, looks[k]), b.x + bw / 2, b.y + 30, 44);
            text(r_, b.x + bw / 2, b.y + 58, looks[k], 1.f, cur ? col::text : col::dim, Align::Center);
        }
    }
    y += 80;

    // Colour: swatches, the hex code, and "usual" for the look's own colour.
    label("Colour");
    {
        static const char* swatches[] = {"d6403a", "e07a2a", "e8c040", "7cb84a", "3f8a3c", "8a4fb0",
                                         "c8507a", "8a5a34", "f0e6c8", "5a6cc8"};
        const float sw = 26.f;
        for (int k = 0; k < 10; ++k) {
            SDL_FRect b{fx + k * (sw + 6), y + 4, sw, sw};
            SDL_Color col = croplook::parseColor(swatches[k], c.look);
            bool cur = c.color == swatches[k];
            if (ui_.hovered(b) && in_.released && !cur) {
                pushUndo();
                C().color = swatches[k];
            }
            fillRound(r_, {b.x - 2, b.y - 2, b.w + 4, b.h + 4}, 6, cur ? col::accent : (ui_.hovered(b) ? col::text : col::panelLine));
            fillRound(r_, b, 5, col);
        }
    }
    y += 42;
    ui_.textField("c_color", {fx, y, 140, fh}, c.color, [this, C](const std::string& v) {
        std::string hex;
        for (char ch : v)
            if (ch != '#' && !std::isspace(static_cast<unsigned char>(ch))) hex += static_cast<char>(std::tolower(static_cast<unsigned char>(ch)));
        if (!croplook::isValidColor(hex)) return toast("A colour is 6 hex digits, e.g. e05040", true);
        pushUndo();
        C().color = hex;
    }, false, "usual");
    if (ui_.button("c_usual", {fx + 150, y, 110, fh}, "Usual", !c.color.empty(), false, 1.5f)) {
        pushUndo();
        C().color.clear();
    }
    y += fh + 10;

    // Problems with this crop.
    auto problems = techdata::validateCrops(crops_, techs_);
    std::vector<std::string> mine;
    for (const auto& p : problems)
        if (p.rfind("crop " + c.id + ":", 0) == 0) mine.push_back(p.substr(p.find(':') + 2));
    heading(r_, x, y, w, mine.empty() ? "Problems: none" : "Problems");
    for (const auto& p : mine)
        for (const auto& ln : wrap(p, static_cast<size_t>(w / 12))) {
            text(r_, x, y, ln, 1.5f, col::bad);
            y += 20;
        }
    if (mine.empty()) {
        text(r_, x, y, "Looks good.", 1.5f, col::good);
        y += 26;
    }
    y += 20;
}

void Editor::drawTreePanel(float x, float& y, float w) {
    text(r_, x, y, "TECH TREE", 1.5f, col::dim);
    y += 26;
    text(r_, x, y, strf("%d techs", static_cast<int>(techs_.size())), 2.5f, col::text);
    y += 40;
    text(r_, x, y, "Click a tech to edit it.", 1.75f, col::dim);
    y += 34;

    drawCropsSection(x, y, w);
    drawTreeTotals(x, y, w);

    auto problems = techdata::validate(techs_);
    for (const auto& p : techdata::validateCrops(crops_, techs_)) problems.push_back(p);
    heading(r_, x, y, w, problems.empty() && fileErrors_.empty() ? "Problems: none" : "Problems");
    for (const auto& fe : fileErrors_) {
        for (const auto& ln : wrap("File: " + fe, static_cast<size_t>(w / 12))) {
            text(r_, x, y, ln, 1.5f, col::bad);
            y += 20;
        }
    }
    for (size_t k = 0; k < problems.size(); ++k) {
        // Click a problem to jump to that tech.
        std::string id = problems[k].substr(0, problems[k].find(':'));
        int j = indexOf(id), c = -1;
        if (id.rfind("crop ", 0) == 0)
            for (int q = 0; q < static_cast<int>(crops_.size()); ++q)
                if (crops_[q].id == id.substr(5)) c = q;
        if (ui_.button("pr" + std::to_string(k), {x, y, w, 30}, fit(problems[k], 1.5f, w - 16), true, false, 1.5f)) {
            if (c >= 0) {
                crop_ = c;
                panelScroll_ = 0;
                return;
            }
            if (j >= 0) {
                sel_ = j;
                return;
            }
        }
        y += 34;
    }
    if (problems.empty() && fileErrors_.empty()) {
        text(r_, x, y, "Everything checks out.", 1.5f, col::good);
        y += 26;
    }

    heading(r_, x, y, w, "How it works");
    const char* lines[] = {
        "Each tech has effects that change the farm's",
        "stats (day length, pick time, coin value...).",
        "Its box only appears in the game once one of",
        "the techs it requires has been bought.",
        "",
        "Save writes include/TechTreeData.h, which is",
        "compiled into the game: rebuild to see changes.",
        "",
        "Double-click empty space   new tech",
        "Drag a tech                move it",
        "Shift+click another tech   add/remove requirement",
        "Delete                     delete selected",
        "Arrow keys                 nudge selected",
        "Ctrl+D                     duplicate",
        "Ctrl+Z / Ctrl+Y            undo / redo",
        "Ctrl+S                     save",
        "Right-drag / wheel         pan / zoom",
        "(On a Mac, Cmd works instead of Ctrl.)",
    };
    for (const char* l : lines) {
        text(r_, x, y, l, 1.25f, col::dim);
        y += 18;
    }
    y += 20;
}

// ===========================================================================
// Drawing: "are you sure?" boxes
// ===========================================================================

// Each stat at the start of the game and with every tech fully bought, so you
// can see where the tree ends up. Crops at once can never be more than the
// patch has room for, so a mismatch between the two is pointed out.
void Editor::drawTreeTotals(float x, float& y, float w) {
    Stats start, full;
    for (const auto& t : techs_) techdata::applyEffects(t, full, t.maxLevel); // same order as the game
    const auto& list = techdata::stats();
    heading(r_, x, y, w, "With everything bought");
    for (int s = 0; s < static_cast<int>(list.size()); ++s) {
        std::string a = techdata::formatStat(s, start), b = techdata::formatStat(s, full);
        bool changed = a != b;
        text(r_, x, y, list[s].label, 1.5f, changed ? col::dim : col::faint);
        y += 17;
        text(r_, x + 16, y, fit(changed ? a + " -> " + b : a + "  (no tech changes this)", 1.25f, w - 16), 1.25f,
             changed ? col::text : col::faint);
        y += 21;
    }
    const int room = full.patchSize * full.patchSize;
    std::string note;
    if (full.maxCrops > room)
        note = strf("Crops reach %d but the patch only has room for %d, so %d of them never get planted.", full.maxCrops,
                    room, full.maxCrops - room);
    else if (full.maxCrops < room)
        note = strf("The patch has room for %d but only %d crops grow at once, so %d spaces always stay empty.", room,
                    full.maxCrops, room - full.maxCrops);
    if (!note.empty()) {
        y += 4;
        for (const auto& ln : wrap(note, static_cast<size_t>(w / 12))) {
            text(r_, x, y, ln, 1.5f, col::accent);
            y += 20;
        }
    }
    y += 6;
}

// The list that opens when you click an effect's stat.
void Editor::drawStatPicker() {
    const auto& list = techdata::stats();
    bool valid = pickTech_ >= 0 && pickTech_ < static_cast<int>(techs_.size()) && pickEffect_ >= 0 &&
                 pickEffect_ < static_cast<int>(techs_[pickTech_].effects.size());
    if (!valid) {
        modal_ = Modal::None;
        return;
    }
    const std::string current = techs_[pickTech_].effects[pickEffect_].stat;
    const float colW = 640.f;
    float rowH = 52.f;
    const int n = static_cast<int>(list.size());
    // One column if it fits the window, otherwise two side by side; in a
    // small window the rows get shorter (and drop their help line) instead.
    int cols = (80.f + rowH * n + 70.f <= winH_ - 40.f || winW_ < colW * 2 + 72.f) ? 1 : 2;
    int perCol = (n + cols - 1) / cols;
    rowH = std::clamp((winH_ - 40.f - 150.f) / perCol, 30.f, 52.f);
    const bool showHelp = rowH >= 46.f;
    float w = std::min(cols == 1 ? 720.f : colW * 2 + 32.f, winW_ - 40.f);
    float h = std::min(80.f + rowH * perCol + 70.f, winH_ - 40.f);
    float x = (winW_ - w) / 2, y = (winH_ - h) / 2;
    float cw = (w - 32.f) / cols;
    fillRound(r_, {x - 2, y - 2, w + 4, h + 4}, 14, col::accent);
    fillRound(r_, {x, y, w, h}, 12, col::panel);
    text(r_, x + 24, y + 22, "Which stat does this effect change?", 2.f, col::text);

    for (int s = 0; s < n; ++s) {
        int c = s / perCol;
        SDL_FRect row{x + 16 + c * cw, y + 64 + rowH * (s % perCol), cw - (cols > 1 ? 8.f : 0.f), rowH - 6};
        bool isCurrent = current == list[s].key;
        bool over = ui_.hovered(row);
        if (isCurrent || over) fillRound(r_, row, 8, isCurrent ? SDL_Color{70, 56, 30, 255} : SDL_Color{48, 54, 60, 255});
        text(r_, row.x + 12, row.y + (showHelp ? 7.f : (row.h - 14.f) / 2), list[s].label, 1.75f,
             isCurrent ? col::accent : col::text);
        if (showHelp) text(r_, row.x + 12, row.y + 28, fit(list[s].help, 1.25f, row.w - 24), 1.25f, col::dim);
        if (over && in_.released) {
            if (!isCurrent) {
                pushUndo();
                techs_[pickTech_].effects[pickEffect_].stat = list[s].key;
            }
            modal_ = Modal::None;
            return;
        }
    }
    if (ui_.button("m_cancel", {x + w - 176, y + h - 60, 160, 44}, "Cancel")) modal_ = Modal::None;
    if (in_.key(SDLK_ESCAPE)) modal_ = Modal::None;
}

// Grid of every icon to choose from: the tech's default, the built-in
// pictures, and your own images in assets/tree/icons/.
void Editor::drawIconPicker() {
    if (iconTech_ < 0 || iconTech_ >= static_cast<int>(techs_.size())) {
        modal_ = Modal::None;
        return;
    }
    const TechDef& t = techs_[iconTech_];
    struct Option {
        std::string icon;  // what to store ("" = default)
        std::string shows; // icon name drawn
        std::string label;
    };
    std::vector<Option> opts;
    opts.push_back({"", t.id, "Default"});
    for (const auto& b : techdata::builtinIcons())
        if (b.key != t.id) opts.push_back({b.key, b.key, b.label});
    for (const auto& c : crops_) opts.push_back({"crop_" + c.id, "crop_" + c.id, c.name});
    for (const auto& c : customIcons_)
        if (c != t.id) opts.push_back({c, c, c});
    const size_t firstCustom = 1 + techdata::builtinIcons().size() - (techdata::isBuiltinIcon(t.id) ? 1 : 0); // crops, then yours

    const float cellW = 104.f, cellH = 104.f, icon = 60.f;
    float w = std::min(winW_ - 40.f, 16.f + cellW * 10.f + 16.f);
    int cols = std::max(1, static_cast<int>((w - 32.f) / cellW));
    int rows = (static_cast<int>(opts.size()) + cols - 1) / cols;
    float h = std::min(winH_ - 40.f, 70.f + rows * cellH + 30.f + 76.f);
    float x = (winW_ - w) / 2, y = (winH_ - h) / 2;
    fillRound(r_, {x - 2, y - 2, w + 4, h + 4}, 14, col::accent);
    fillRound(r_, {x, y, w, h}, 12, col::panel);
    text(r_, x + 24, y + 22, fit("Icon for " + (t.name.empty() ? t.id : t.name), 2.f, w - 48), 2.f, col::text);

    const std::string current = t.icon;
    for (int k = 0; k < static_cast<int>(opts.size()); ++k) {
        const Option& o = opts[k];
        float cx = x + 16 + (k % cols) * cellW, cy = y + 64 + (k / cols) * cellH;
        if (cy + cellH > y + h - 76) break; // (only with an enormous number of images)
        SDL_FRect cell{cx + 4, cy, cellW - 8, cellH - 6};
        bool isCurrent = o.icon == current || (o.icon.empty() && current == t.id);
        bool over = ui_.hovered(cell);
        if (isCurrent || over) fillRound(r_, cell, 10, isCurrent ? SDL_Color{70, 56, 30, 255} : SDL_Color{48, 54, 60, 255});
        if (static_cast<size_t>(k) == firstCustom) fillRect(r_, {cell.x - 6, cell.y + 6, 2, cell.h - 12}, col::accent);
        drawIcon(o.shows, {cell.x + (cell.w - icon) / 2, cell.y + 8, icon, icon}, o.icon.empty() ? t.name : o.label);
        text(r_, cell.x + cell.w / 2, cell.y + cell.h - 22, fit(o.label, 1.25f, cell.w - 6), 1.25f,
             isCurrent ? col::accent : col::dim, Align::Center);
        if (over && in_.released) {
            if (!isCurrent) {
                pushUndo();
                techs_[iconTech_].icon = o.icon == techs_[iconTech_].id ? "" : o.icon;
            }
            modal_ = Modal::None;
            return;
        }
    }
    text(r_, x + 24, y + h - 64, "Your own icons: 64x64 PNGs in assets/tree/icons/ - the file name is the icon name.", 1.25f, col::dim);
    text(r_, x + 24, y + h - 46, "A file named like a built-in icon (e.g. carrots.png) repaints that one.", 1.25f, col::faint);
    if (ui_.button("ip_rescan", {x + w - 336, y + h - 58, 150, 42}, "Rescan", true, false, 1.5f)) loadIcons();
    if (ui_.button("m_cancel", {x + w - 176, y + h - 58, 160, 42}, "Cancel")) modal_ = Modal::None;
    if (in_.key(SDLK_ESCAPE)) modal_ = Modal::None;
}

void Editor::drawModal() {
    fillRect(r_, {0, 0, static_cast<float>(winW_), static_cast<float>(winH_)}, SDL_Color{0, 0, 0, 160});
    if (modal_ == Modal::StatPicker) return drawStatPicker();
    if (modal_ == Modal::IconPicker) return drawIconPicker();
    float w = 560, h = 190, x = (winW_ - w) / 2, y = (winH_ - h) / 2;
    fillRound(r_, {x - 2, y - 2, w + 4, h + 4}, 14, col::accent);
    fillRound(r_, {x, y, w, h}, 12, col::panel);
    if (modal_ == Modal::Quit) {
        text(r_, x + w / 2, y + 28, "Save your changes?", 2.5f, col::text, Align::Center);
        text(r_, x + w / 2, y + 68, "The tech tree has unsaved changes.", 1.75f, col::dim, Align::Center);
        if (ui_.button("m_save", {x + 20, y + 120, 160, 46}, "Save & quit", true, true)) {
            if (save()) running_ = false;
            modal_ = Modal::None;
        }
        if (ui_.button("m_discard", {x + 200, y + 120, 160, 46}, "Don't save")) running_ = false;
        if (ui_.button("m_cancel", {x + 380, y + 120, 160, 46}, "Cancel")) modal_ = Modal::None;
    } else {
        text(r_, x + w / 2, y + 28, "Reload from disk?", 2.5f, col::text, Align::Center);
        text(r_, x + w / 2, y + 68, "Your unsaved changes will be lost.", 1.75f, col::dim, Align::Center);
        if (ui_.button("m_reload", {x + 60, y + 120, 200, 46}, "Reload", true, true)) {
            load(path_);
            modal_ = Modal::None;
        }
        if (ui_.button("m_cancel", {x + 300, y + 120, 200, 46}, "Cancel")) modal_ = Modal::None;
    }
    if (in_.key(SDLK_ESCAPE)) modal_ = Modal::None;
}

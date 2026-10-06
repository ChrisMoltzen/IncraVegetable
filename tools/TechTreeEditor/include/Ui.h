// Ui.h - a small immediate-mode UI (buttons and text fields) for the editor.
//
// Every frame the editor calls button()/textField() for what's on screen;
// the Ui works out hover, clicks and typing. Text fields only change the
// data when you finish editing (Enter, Tab, or clicking somewhere else),
// through the commit callback - so each edit is one undo step.
#pragma once

#include "Gfx.h"

#include <SDL3/SDL.h>
#include <cstdint>
#include <functional>
#include <string>
#include <vector>

struct Input {
    float mx = -1000.f, my = -1000.f;
    float dx = 0.f, dy = 0.f;    // mouse movement this frame
    bool down = false;           // left button held
    bool pressed = false;        // left button went down this frame
    bool released = false;       // left button came up this frame
    bool doubleClick = false;
    bool rightDown = false, middleDown = false;
    float wheel = 0.f;
    std::string typed;               // text typed this frame
    std::vector<SDL_Keycode> keys;   // keys pressed this frame
    SDL_Keymod mod = SDL_KMOD_NONE;

    bool ctrl() const { return (mod & (SDL_KMOD_CTRL | SDL_KMOD_GUI)) != 0; } // Ctrl, or Cmd on a Mac
    bool shift() const { return (mod & SDL_KMOD_SHIFT) != 0; }
    bool key(SDL_Keycode k) const;
    void clearFrame(); // keeps held buttons, clears per-frame things
};

class Ui {
public:
    using Commit = std::function<void(const std::string&)>;

    void beginFrame(const Input& in, SDL_Renderer* r, SDL_Window* w);
    void endFrame();

    // Returns true when clicked (pressed and released on it).
    bool button(const std::string& id, const SDL_FRect& rc, const std::string& label, bool enabled = true,
                bool highlighted = false, float scale = 1.75f);
    // An editable text box. onCommit runs when editing finishes and the text changed.
    void textField(const std::string& id, const SDL_FRect& rc, const std::string& value, Commit onCommit,
                   bool multiline = false, const char* placeholder = nullptr);
    void numberField(const std::string& id, const SDL_FRect& rc, double value, std::function<void(double)> onCommit,
                     bool integer = false);

    bool editing() const { return focus_ != 0; }
    void commitFocus();
    void cancelFocus();
    bool hovered(const SDL_FRect& rc) const { return gfx::inside(in_.mx, in_.my, rc) && clipOk(); }

    // Widgets outside the clip rect can't be hovered or clicked (used for the scrolling panel).
    void setClip(const SDL_FRect* rc);

    // Set when a number field gets text that isn't a number, so the editor can say so.
    std::string lastError;

private:
    static uint64_t hash(const std::string& s);
    void focus(uint64_t id, const std::string& value, Commit commit, const SDL_FRect& rc, bool multiline);
    bool clipOk() const { return !clipOn_ || gfx::inside(in_.mx, in_.my, clip_); }

    Input in_;
    SDL_Renderer* r_ = nullptr;
    SDL_Window* win_ = nullptr;
    uint64_t pressedId_ = 0;

    // The field being edited.
    uint64_t focus_ = 0;
    std::string buf_;
    std::string original_;
    size_t caret_ = 0;
    bool allSelected_ = false;
    bool multiline_ = false;
    Commit commit_;
    SDL_FRect focusRect_{};
    bool focusSeen_ = false; // drawn this frame?
    bool freshFocus_ = false; // focused this frame: don't also handle this frame's keys (e.g. the Tab that got us here)

    // Tab moves to the next field.
    std::vector<uint64_t> order_, lastOrder_;
    uint64_t tabTo_ = 0;
    float blink_ = 0.f;

    bool clipOn_ = false;
    SDL_FRect clip_{};
};

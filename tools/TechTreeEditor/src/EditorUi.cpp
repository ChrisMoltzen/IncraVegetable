#include "EditorUi.h"

#include <algorithm>
#include <cmath>
#include <cstdlib>

using namespace gfx;

bool Input::key(SDL_Keycode k) const { return std::find(keys.begin(), keys.end(), k) != keys.end(); }

void Input::clearFrame() {
    dx = dy = 0.f;
    pressed = released = doubleClick = false;
    wheel = 0.f;
    typed.clear();
    keys.clear();
}

uint64_t Ui::hash(const std::string& s) {
    uint64_t h = 1469598103934665603ull;
    for (unsigned char c : s) h = (h ^ c) * 1099511628211ull;
    return h ? h : 1;
}

void Ui::setClip(const SDL_FRect* rc) {
    clipOn_ = rc != nullptr;
    if (rc) {
        clip_ = *rc;
        SDL_Rect ir{static_cast<int>(rc->x), static_cast<int>(rc->y), static_cast<int>(rc->w), static_cast<int>(rc->h)};
        SDL_SetRenderClipRect(r_, &ir);
    } else {
        SDL_SetRenderClipRect(r_, nullptr);
    }
}

void Ui::beginFrame(const Input& in, SDL_Renderer* r, SDL_Window* w) {
    in_ = in;
    r_ = r;
    win_ = w;
    blink_ += 1.f / 60.f;
    lastOrder_ = order_;
    order_.clear();
    focusSeen_ = false;
    freshFocus_ = false;
    lastError.clear();
    // Clicking anywhere outside the field being edited finishes the edit.
    if (focus_ && in_.pressed && !inside(in_.mx, in_.my, focusRect_)) commitFocus();
}

void Ui::endFrame() {
    // The field being edited disappeared (e.g. the tech was deselected): keep the edit.
    if (focus_ && !focusSeen_) commitFocus();
    if (!in_.down) pressedId_ = 0;
}

void Ui::focus(uint64_t id, const std::string& value, Commit commit, const SDL_FRect& rc, bool multiline) {
    if (focus_ && focus_ != id) commitFocus();
    focus_ = id;
    buf_ = original_ = value;
    caret_ = buf_.size();
    allSelected_ = true; // typing replaces the whole value, like most apps
    multiline_ = multiline;
    commit_ = std::move(commit);
    focusRect_ = rc;
    blink_ = 0.f;
    freshFocus_ = true;
    SDL_StartTextInput(win_);
}

void Ui::commitFocus() {
    if (!focus_) return;
    focus_ = 0;
    SDL_StopTextInput(win_);
    if (buf_ != original_ && commit_) {
        Commit c = std::move(commit_);
        c(buf_);
    }
    commit_ = nullptr;
}

void Ui::cancelFocus() {
    focus_ = 0;
    commit_ = nullptr;
    SDL_StopTextInput(win_);
}

bool Ui::button(const std::string& idStr, const SDL_FRect& rc, const std::string& label, bool enabled,
                bool highlighted, float scale) {
    uint64_t id = hash(idStr);
    bool over = enabled && hovered(rc);
    if (over && in_.pressed) pressedId_ = id;
    bool clicked = over && in_.released && pressedId_ == id;

    SDL_Color fill = !enabled       ? SDL_Color{44, 47, 50, 255}
                     : highlighted  ? SDL_Color{200, 130, 40, 255}
                     : over         ? SDL_Color{78, 86, 94, 255}
                                    : SDL_Color{58, 64, 70, 255};
    if (over && in_.down && pressedId_ == id) fill = SDL_Color{95, 104, 112, 255};
    fillRound(r_, rc, 6.f, fill);
    text(r_, rc.x + rc.w / 2, rc.y + (rc.h - 8.f * scale) / 2, fit(label, scale, rc.w - 8), scale,
         enabled ? col::text : col::faint, Align::Center);
    return clicked;
}

void Ui::textField(const std::string& idStr, const SDL_FRect& rc, const std::string& value, Commit onCommit,
                   bool multiline, const char* placeholder) {
    uint64_t id = hash(idStr);
    order_.push_back(id);
    const float scale = 2.f;

    if (tabTo_ == id) {
        tabTo_ = 0;
        focus(id, value, onCommit, rc, multiline);
    }
    if (hovered(rc) && in_.pressed && focus_ != id) focus(id, value, onCommit, rc, multiline);

    bool focused = focus_ == id;
    if (focused) {
        focusSeen_ = true;
        focusRect_ = rc;
        commit_ = onCommit; // refresh: the data it points at may have moved
        // Typing.
        if (!freshFocus_ && !in_.typed.empty()) {
            if (allSelected_) {
                buf_.clear();
                caret_ = 0;
                allSelected_ = false;
            }
            buf_.insert(caret_, in_.typed);
            caret_ += in_.typed.size();
        }
        for (SDL_Keycode k : freshFocus_ ? std::vector<SDL_Keycode>{} : in_.keys) {
            if (k == SDLK_BACKSPACE) {
                if (allSelected_) {
                    buf_.clear();
                    caret_ = 0;
                } else if (caret_ > 0) {
                    buf_.erase(--caret_, 1);
                }
                allSelected_ = false;
            } else if (k == SDLK_DELETE) {
                if (allSelected_) {
                    buf_.clear();
                    caret_ = 0;
                } else if (caret_ < buf_.size()) {
                    buf_.erase(caret_, 1);
                }
                allSelected_ = false;
            } else if (k == SDLK_LEFT && !multiline_) {
                if (allSelected_) caret_ = 0;
                else if (caret_ > 0) --caret_;
                allSelected_ = false;
            } else if (k == SDLK_RIGHT && !multiline_) {
                if (!allSelected_ && caret_ < buf_.size()) ++caret_;
                allSelected_ = false;
            } else if (k == SDLK_HOME && !multiline_) {
                caret_ = 0;
                allSelected_ = false;
            } else if (k == SDLK_END) {
                caret_ = buf_.size();
                allSelected_ = false;
            } else if (k == SDLK_A && in_.ctrl()) {
                allSelected_ = true;
            } else if (k == SDLK_V && in_.ctrl()) {
                if (char* clip = SDL_GetClipboardText()) {
                    std::string paste = clip;
                    SDL_free(clip);
                    for (char& c : paste)
                        if (c == '\n' || c == '\r' || c == '\t') c = ' ';
                    if (allSelected_) {
                        buf_.clear();
                        caret_ = 0;
                        allSelected_ = false;
                    }
                    buf_.insert(caret_, paste);
                    caret_ += paste.size();
                }
            } else if (k == SDLK_C && in_.ctrl()) {
                SDL_SetClipboardText(buf_.c_str());
            } else if (k == SDLK_ESCAPE) {
                cancelFocus();
                return;
            } else if (k == SDLK_RETURN || k == SDLK_KP_ENTER || k == SDLK_TAB) {
                if (k == SDLK_TAB) {
                    // Find the next (or previous with Shift) field from last frame's order.
                    auto it = std::find(lastOrder_.begin(), lastOrder_.end(), id);
                    if (it != lastOrder_.end() && lastOrder_.size() > 1) {
                        long i = static_cast<long>(it - lastOrder_.begin());
                        long n = static_cast<long>(lastOrder_.size());
                        tabTo_ = lastOrder_[static_cast<size_t>(((i + (in_.shift() ? -1 : 1)) % n + n) % n)];
                    }
                }
                commitFocus();
                focused = false;
                break;
            }
        }
        if (multiline_) caret_ = buf_.size();
    }

    // Draw.
    fillRound(r_, rc, 5.f, focused ? SDL_Color{240, 160, 50, 255} : (hovered(rc) ? SDL_Color{80, 88, 96, 255} : SDL_Color{56, 62, 68, 255}));
    SDL_FRect in{rc.x + 2, rc.y + 2, rc.w - 4, rc.h - 4};
    fillRound(r_, in, 4.f, col::field);
    const std::string& shown = focused ? buf_ : value;
    float pad = 8.f, glyph = 8.f * scale;
    size_t maxChars = static_cast<size_t>(std::max(1.f, (in.w - pad * 2) / glyph));

    if (shown.empty() && !focused && placeholder) {
        text(r_, in.x + pad, in.y + (multiline ? pad : (in.h - glyph) / 2), placeholder, 1.5f, col::faint);
    } else if (multiline) {
        auto lines = wrap(shown, maxChars);
        if (lines.empty()) lines.push_back("");
        int maxLines = std::max(1, static_cast<int>((in.h - pad) / (glyph + 4)));
        int first = std::max(0, static_cast<int>(lines.size()) - maxLines); // keep the end (where you type) visible
        float y = in.y + pad;
        for (int i = first; i < static_cast<int>(lines.size()); ++i) {
            if (focused && allSelected_) fillRect(r_, {in.x + pad, y - 2, textWidth(lines[i], scale), glyph + 4}, SDL_Color{70, 100, 150, 255});
            text(r_, in.x + pad, y, lines[i], scale, col::text);
            y += glyph + 4;
        }
        if (focused && std::fmod(blink_, 1.f) < 0.6f) {
            float cx = in.x + pad + textWidth(lines.back(), scale) + 1;
            fillRect(r_, {cx, y - glyph - 6, 2, glyph + 4}, col::text);
        }
    } else {
        // Scroll so the caret stays visible.
        size_t start = 0;
        if (focused && caret_ > maxChars) start = caret_ - maxChars;
        std::string vis = shown.substr(start, maxChars);
        float ty = in.y + (in.h - glyph) / 2;
        if (focused && allSelected_ && !vis.empty())
            fillRect(r_, {in.x + pad, ty - 2, textWidth(vis, scale), glyph + 4}, SDL_Color{70, 100, 150, 255});
        text(r_, in.x + pad, ty, vis, scale, col::text);
        if (focused && std::fmod(blink_, 1.f) < 0.6f) {
            float cx = in.x + pad + textWidth(shown.substr(start, caret_ - start), scale);
            fillRect(r_, {cx, ty - 2, 2, glyph + 4}, col::text);
        }
    }
}

void Ui::numberField(const std::string& id, const SDL_FRect& rc, double value, std::function<void(double)> onCommit,
                     bool integer) {
    std::string shown = integer ? gfx::strf("%d", static_cast<int>(std::lround(value))) : gfx::strf("%.7g", value); // 7 digits: enough for a float, without 1.899999976
    textField(id, rc, shown, [this, onCommit, integer](const std::string& s) {
        char* end = nullptr;
        double v = std::strtod(s.c_str(), &end);
        if (s.empty() || !end || *end != '\0' || !std::isfinite(v)) {
            lastError = "'" + s + "' isn't a number";
            return;
        }
        onCommit(integer ? std::round(v) : v);
    });
}

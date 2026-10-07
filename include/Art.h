// Art.h - swappable artwork.
//
// Every picture in the game has a name such as "crops/carrot" or "ui/panel".
// When drawing, the game looks for assets/<name>.png (also .jpg .bmp .tga).
// If the file is there it is drawn; if not, the built-in drawing is used.
// So artwork can be added one piece at a time and the game always works.
//
// The full list of names, sizes and what each one is for is in
// ArtCatalog.cpp. Running the game with --export-art-templates writes every
// built-in drawing out as a PNG (at the right size, with the right name)
// plus ART_LIST.md, ready to paint over.
#pragma once

#include <SDL3/SDL.h>
#include <functional>
#include <initializer_list>
#include <string>
#include <utility>
#include <vector>

namespace art {

using DrawFn = std::function<void(SDL_Renderer*, const SDL_FRect&)>;

struct Info {
    std::string name;        // file name without extension, relative to assets/
    int width = 128;         // recommended image size in pixels
    int height = 128;
    std::string description; // shown in ART_LIST.md
    bool nineSlice = false;  // stretchy UI piece: corners keep their size, the middle stretches
    DrawFn fallback;         // built-in drawing used when there is no image
    DrawFn templateDraw;     // optional: what to put in the template instead of the fallback
};

// Call once after the renderer exists. techNodes = (id, name) of every
// tech tree upgrade, so each one can have an icon.
void init(SDL_Renderer* renderer, const std::vector<std::pair<std::string, std::string>>& techNodes);
void shutdown();

// The folder images are loaded from ("" if none was found).
const std::string& assetDir();

void add(Info info);
const std::vector<Info>& catalog();

// The texture for `name`, or nullptr if no image was provided.
SDL_Texture* texture(const std::string& name);
bool has(const std::string& name);

// Draws the image, or the built-in fallback if there is no image.
void draw(SDL_Renderer* r, const std::string& name, const SDL_FRect& dst, Uint8 alpha = 255);

// Like draw(), but mirrored left-to-right when flipX is true (for characters
// that face the way they walk). The built-in fallback is never mirrored.
void drawFlipped(SDL_Renderer* r, const std::string& name, const SDL_FRect& dst, bool flipX);

// Draws the first of `names` that has an image; if none do, the fallback of the last.
void drawFirst(SDL_Renderer* r, std::initializer_list<std::string> names, const SDL_FRect& dst);

// For things with states (variant = "_hover", "_pressed", "_disabled" or "").
// Uses name+variant if that image exists; otherwise the base image with an
// automatic highlight/darken; otherwise the built-in drawing of the variant.
void drawVariant(SDL_Renderer* r, const std::string& name, const std::string& variant, const SDL_FRect& dst);

// Draws the image multiplied by a colour. Returns false if there's no image.
bool drawTinted(SDL_Renderer* r, const std::string& name, const SDL_FRect& dst, SDL_Color tint);

// Draws the left `fraction` (0..1) of the image, for bars. Returns false if there's no image.
bool drawFill(SDL_Renderer* r, const std::string& name, const SDL_FRect& dst, float fraction);

// 9-slice corners are drawn this many times their usual size while one of
// these is alive (e.g. the tech tree's zoom), so a zoomed-out tile shrinks as
// a whole instead of keeping full-size corners around a tiny middle.
class SliceScale {
public:
    explicit SliceScale(float scale);
    ~SliceScale();
    SliceScale(const SliceScale&) = delete;
    SliceScale& operator=(const SliceScale&) = delete;

private:
    float previous_;
};
float sliceScale(); // the current multiplier (1 normally); built-in drawings can follow it too

// Options from assets/art.txt, e.g. "menu_crop_rows off".
bool option(const std::string& key, bool defaultValue);

// Forgets all loaded images and art.txt so they are read again (F5).
// Returns how many images were found.
int reload();
int loadedCount();
// Image files in assets/ that don't match any art name (probably typos).
const std::vector<std::string>& unknownFiles();

// Writes every catalog entry, drawn by its built-in drawing, as a PNG into
// `folder` (same sub-folders as assets/), plus ART_LIST.md. Returns files written.
int exportTemplates(SDL_Renderer* r, const std::string& folder);

// Registers every art slot (ArtCatalog.cpp).
void registerCatalog(const std::vector<std::pair<std::string, std::string>>& techNodes);

} // namespace art

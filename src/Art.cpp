#include "Art.h"

#include "Draw.h"

#include <algorithm>
#include <cstring>
#include <functional>
#include <cctype>
#include <map>
#include <sstream>
#include <unordered_map>

// stb is third-party code; don't let its style trip our warning flags.
#if defined(__GNUC__)
#pragma GCC diagnostic push
#pragma GCC diagnostic ignored "-Wunused-function"
#pragma GCC diagnostic ignored "-Wmissing-field-initializers"
#pragma GCC diagnostic ignored "-Wsign-compare"
#pragma GCC diagnostic ignored "-Wunused-parameter"
#pragma GCC diagnostic ignored "-Wdeprecated-declarations" // macOS marks sprintf deprecated (used in stb's HDR writer)
#elif defined(_MSC_VER)
#pragma warning(push, 0)
#endif
#define STB_IMAGE_IMPLEMENTATION
#define STBI_ONLY_PNG
#define STBI_ONLY_JPEG
#define STBI_ONLY_BMP
#define STBI_ONLY_TGA
#include "stb_image.h"

#define STB_IMAGE_WRITE_IMPLEMENTATION
#include "stb_image_write.h"
#if defined(__GNUC__)
#pragma GCC diagnostic pop
#elif defined(_MSC_VER)
#pragma warning(pop)
#endif

namespace art {

namespace {

struct Entry {
    SDL_Texture* texture = nullptr;
    bool tried = false;
};

struct Slice {
    float corner = -1.f; // pixels in the image; -1 = a quarter of the smaller side
    float scale = 1.f;   // how big corners are drawn on screen, relative to their pixels
};

struct State {
    SDL_Renderer* renderer = nullptr;
    std::string dir;
    std::vector<Info> catalog;
    std::unordered_map<std::string, size_t> index;
    std::unordered_map<std::string, Entry> cache;
    // From art.txt
    bool configLoaded = false;
    SDL_ScaleMode defaultFilter = SDL_SCALEMODE_LINEAR;
    std::map<std::string, SDL_ScaleMode> filters;
    std::map<std::string, Slice> slices;
    std::map<std::string, bool> options;
    std::vector<std::string> unknownFiles;
};

State& S() {
    static State s;
    return s;
}

const char* kExtensions[] = {".png", ".jpg", ".jpeg", ".bmp", ".tga"};

bool dirExists(const std::string& path) {
    SDL_PathInfo info;
    return SDL_GetPathInfo(path.c_str(), &info) && info.type == SDL_PATHTYPE_DIRECTORY;
}

// Where to look for assets/, in order:
//  1. the project's own assets folder (desktop builds only), so edits show
//     up straight away with F5 and no rebuild;
//  2. assets/ next to the executable (or inside the app bundle on iOS/macOS);
//  3. assets/ in the current working directory.
std::string findAssetDir() {
    std::vector<std::string> candidates;
#ifdef INCRA_SOURCE_ASSETS_DIR
    candidates.push_back(std::string(INCRA_SOURCE_ASSETS_DIR) + "/");
#endif
    if (const char* base = SDL_GetBasePath()) candidates.push_back(std::string(base) + "assets/");
    candidates.push_back("assets/");
    for (const auto& c : candidates)
        if (dirExists(c)) return c;
    return "";
}

void loadConfig() {
    State& s = S();
    s.configLoaded = true;
    s.defaultFilter = SDL_SCALEMODE_LINEAR;
    s.filters.clear();
    s.slices.clear();
    s.options.clear();
    s.filters["ui/font"] = SDL_SCALEMODE_NEAREST; // crisp text unless told otherwise
    if (s.dir.empty()) return;

    size_t size = 0;
    void* data = SDL_LoadFile((s.dir + "art.txt").c_str(), &size);
    if (!data) return;
    std::istringstream in(std::string(static_cast<const char*>(data), size));
    SDL_free(data);

    auto parseFilter = [](const std::string& v) {
        return (v == "nearest" || v == "pixel" || v == "pixelart") ? SDL_SCALEMODE_NEAREST : SDL_SCALEMODE_LINEAR;
    };
    std::string line;
    while (std::getline(in, line)) {
        if (auto hash = line.find('#'); hash != std::string::npos) line.resize(hash);
        std::istringstream ss(line);
        std::string key;
        if (!(ss >> key)) continue;
        if (key == "filter") {
            std::string a, b;
            ss >> a >> b;
            if (b.empty()) s.defaultFilter = parseFilter(a);
            else s.filters[a] = parseFilter(b);
        } else if (key == "slice") {
            std::string name;
            Slice sl;
            ss >> name >> sl.corner;
            if (!(ss >> sl.scale)) sl.scale = 1.f;
            s.slices[name] = sl;
        } else {
            std::string v;
            ss >> v;
            s.options[key] = !(v == "off" || v == "0" || v == "false" || v == "no");
        }
    }
}

SDL_Texture* loadTexture(const std::string& name) {
    State& s = S();
    if (s.dir.empty() || !s.renderer) return nullptr;
    for (const char* ext : kExtensions) {
        std::string path = s.dir + name + ext;
        size_t size = 0;
        void* file = SDL_LoadFile(path.c_str(), &size);
        if (!file) continue;
        int w = 0, h = 0, channels = 0;
        unsigned char* pixels = stbi_load_from_memory(static_cast<const stbi_uc*>(file), static_cast<int>(size), &w,
                                                      &h, &channels, 4);
        SDL_free(file);
        if (!pixels) {
            SDL_Log("Art: couldn't read %s (%s)", path.c_str(), stbi_failure_reason());
            return nullptr;
        }
        SDL_Surface* surf = SDL_CreateSurfaceFrom(w, h, SDL_PIXELFORMAT_RGBA32, pixels, w * 4);
        SDL_Texture* tex = surf ? SDL_CreateTextureFromSurface(s.renderer, surf) : nullptr;
        if (surf) SDL_DestroySurface(surf);
        stbi_image_free(pixels);
        if (!tex) {
            SDL_Log("Art: couldn't create texture for %s: %s", path.c_str(), SDL_GetError());
            return nullptr;
        }
        SDL_SetTextureBlendMode(tex, SDL_BLENDMODE_BLEND);
        auto f = s.filters.find(name);
        SDL_SetTextureScaleMode(tex, f != s.filters.end() ? f->second : s.defaultFilter);
        return tex;
    }
    return nullptr;
}

const Info* infoFor(const std::string& name) {
    auto it = S().index.find(name);
    return it == S().index.end() ? nullptr : &S().catalog[it->second];
}

void renderTexture(SDL_Renderer* r, const std::string& name, SDL_Texture* tex, const SDL_FRect& dst) {
    const Info* info = infoFor(name);
    if (info && info->nineSlice) {
        float tw = 0, th = 0;
        SDL_GetTextureSize(tex, &tw, &th);
        Slice sl;
        if (auto it = S().slices.find(name); it != S().slices.end()) sl = it->second;
        float corner = sl.corner > 0 ? sl.corner : std::floor(std::min(tw, th) / 4.f);
        corner = std::min({corner, tw / 2.f - 1.f, th / 2.f - 1.f});
        // Shrink the corners if the box is too small to fit them.
        float scale = std::min({sl.scale, dst.w / (2.f * corner), dst.h / (2.f * corner)});
        if (corner > 0 && scale > 0) {
            SDL_RenderTexture9Grid(r, tex, nullptr, corner, corner, corner, corner, scale, &dst);
            return;
        }
    }
    SDL_RenderTexture(r, tex, nullptr, &dst);
}

void drawFallback(SDL_Renderer* r, const std::string& name, const SDL_FRect& dst) {
    if (const Info* info = infoFor(name); info && info->fallback) info->fallback(r, dst);
}

} // namespace

// ---------------------------------------------------------------------------

void init(SDL_Renderer* renderer, const std::vector<std::pair<std::string, std::string>>& techNodes) {
    State& s = S();
    s.renderer = renderer;
    s.dir = findAssetDir();
    if (s.catalog.empty()) registerCatalog(techNodes);
    int n = reload();
    if (s.dir.empty()) SDL_Log("Art: no assets folder found - using built-in art");
    else SDL_Log("Art: %d image(s) loaded from %s", n, s.dir.c_str());
}

void shutdown() {
    for (auto& [name, e] : S().cache)
        if (e.texture) SDL_DestroyTexture(e.texture);
    S().cache.clear();
}

const std::string& assetDir() { return S().dir; }

void add(Info info) {
    State& s = S();
    if (s.index.count(info.name)) return;
    s.index[info.name] = s.catalog.size();
    s.catalog.push_back(std::move(info));
}

const std::vector<Info>& catalog() { return S().catalog; }

SDL_Texture* texture(const std::string& name) {
    State& s = S();
    if (!s.configLoaded) loadConfig();
    Entry& e = s.cache[name];
    if (!e.tried) {
        e.tried = true;
        e.texture = loadTexture(name);
    }
    return e.texture;
}

bool has(const std::string& name) { return texture(name) != nullptr; }

void draw(SDL_Renderer* r, const std::string& name, const SDL_FRect& dst, Uint8 alpha) {
    if (SDL_Texture* tex = texture(name)) {
        SDL_SetTextureAlphaMod(tex, alpha);
        renderTexture(r, name, tex, dst);
        SDL_SetTextureAlphaMod(tex, 255);
    } else {
        drawFallback(r, name, dst);
    }
}

void drawFirst(SDL_Renderer* r, std::initializer_list<std::string> names, const SDL_FRect& dst) {
    for (const auto& n : names) {
        if (SDL_Texture* tex = texture(n)) {
            renderTexture(r, n, tex, dst);
            return;
        }
    }
    if (names.size()) drawFallback(r, *(names.end() - 1), dst);
}

void drawVariant(SDL_Renderer* r, const std::string& name, const std::string& variant, const SDL_FRect& dst) {
    if (variant.empty()) return draw(r, name, dst);
    std::string full = name + variant;
    if (SDL_Texture* tex = texture(full)) return renderTexture(r, full, tex, dst);
    if (SDL_Texture* base = texture(name)) {
        // No special image for this state: adjust the normal one.
        if (variant == "_hover") {
            renderTexture(r, name, base, dst);
            SDL_SetTextureBlendMode(base, SDL_BLENDMODE_ADD);
            SDL_SetTextureAlphaMod(base, 45);
            renderTexture(r, name, base, dst);
            SDL_SetTextureAlphaMod(base, 255);
            SDL_SetTextureBlendMode(base, SDL_BLENDMODE_BLEND);
        } else {
            Uint8 v = variant == "_disabled" ? 140 : 200;
            SDL_SetTextureColorMod(base, v, v, v);
            renderTexture(r, name, base, dst);
            SDL_SetTextureColorMod(base, 255, 255, 255);
        }
        return;
    }
    if (infoFor(full)) drawFallback(r, full, dst);
    else drawFallback(r, name, dst);
}

bool drawTinted(SDL_Renderer* r, const std::string& name, const SDL_FRect& dst, SDL_Color tint) {
    SDL_Texture* tex = texture(name);
    if (!tex) return false;
    SDL_SetTextureColorMod(tex, tint.r, tint.g, tint.b);
    SDL_SetTextureAlphaMod(tex, tint.a);
    renderTexture(r, name, tex, dst);
    SDL_SetTextureColorMod(tex, 255, 255, 255);
    SDL_SetTextureAlphaMod(tex, 255);
    return true;
}

bool drawFill(SDL_Renderer* r, const std::string& name, const SDL_FRect& dst, float fraction) {
    SDL_Texture* tex = texture(name);
    if (!tex) return false;
    fraction = std::clamp(fraction, 0.f, 1.f);
    if (fraction <= 0.f) return true;
    float tw = 0, th = 0;
    SDL_GetTextureSize(tex, &tw, &th);
    SDL_FRect src{0, 0, tw * fraction, th};
    SDL_FRect d{dst.x, dst.y, dst.w * fraction, dst.h};
    SDL_RenderTexture(r, tex, &src, &d);
    return true;
}

bool option(const std::string& key, bool defaultValue) {
    if (!S().configLoaded) loadConfig();
    auto it = S().options.find(key);
    return it == S().options.end() ? defaultValue : it->second;
}

// Lists image files in assets/ whose names don't match any art slot - usually a typo.
static void findUnknownFiles() {
    State& s = S();
    s.unknownFiles.clear();
    if (s.dir.empty()) return;
    struct Ctx {
        std::string root;
        std::vector<std::string> found;
    } ctx{s.dir, {}};
    // Recursive walk; SDL hands us each entry of each folder.
    std::function<void(const std::string&)> walk = [&](const std::string& rel) {
        struct Inner {
            std::function<void(const std::string&)>* walk;
            Ctx* ctx;
            std::string rel;
        } inner{&walk, &ctx, rel};
        SDL_EnumerateDirectory((ctx.root + rel).c_str(),
            [](void* user, const char* dirname, const char* fname) -> SDL_EnumerationResult {
                auto* in = static_cast<Inner*>(user);
                std::string relPath = in->rel + fname;
                SDL_PathInfo info;
                if (SDL_GetPathInfo((std::string(dirname) + fname).c_str(), &info) && info.type == SDL_PATHTYPE_DIRECTORY) {
                    (*in->walk)(relPath + "/");
                } else {
                    in->ctx->found.push_back(relPath);
                }
                return SDL_ENUM_CONTINUE;
            },
            &inner);
    };
    walk("");
    for (const auto& f : ctx.found) {
        std::string lower = f;
        for (char& c : lower) c = static_cast<char>(std::tolower(static_cast<unsigned char>(c)));
        bool image = false;
        std::string stem;
        for (const char* ext : kExtensions) {
            size_t n = std::strlen(ext);
            if (lower.size() > n && lower.compare(lower.size() - n, n, ext) == 0) {
                image = true;
                stem = f.substr(0, f.size() - n);
            }
        }
        if (image && !s.index.count(stem)) {
            s.unknownFiles.push_back(f);
            SDL_Log("Art: %s doesn't match any art name (see ART_LIST.md) - it won't be used", f.c_str());
        }
    }
}

int reload() {
    State& s = S();
    shutdown();
    if (s.dir.empty()) s.dir = findAssetDir();
    loadConfig();
    findUnknownFiles();
    return loadedCount();
}

int loadedCount() {
    int found = 0;
    for (const auto& info : S().catalog)
        if (texture(info.name)) ++found;
    return found;
}

const std::vector<std::string>& unknownFiles() { return S().unknownFiles; }

// ---------------------------------------------------------------------------
// Template export
// ---------------------------------------------------------------------------

int exportTemplates(SDL_Renderer* r, const std::string& folderIn) {
    std::string folder = folderIn;
    if (!folder.empty() && folder.back() != '/' && folder.back() != '\\') folder += '/';
    SDL_CreateDirectory(folder.c_str());

    SDL_Texture* oldTarget = SDL_GetRenderTarget(r);
    int written = 0;
    std::ostringstream list;
    list << "# IncraVegetable art list\n\n"
         << "Put your images in the `assets/` folder using these names (PNG recommended; JPG, BMP and TGA also work).\n"
         << "Any image you leave out uses the built-in art, so you can replace things one at a time.\n"
         << "Sizes are what the game was designed around; other sizes are scaled to fit.\n"
         << "**9-slice** images keep their corners and stretch their middle, so one image fits any size box.\n"
         << "Variants ending in `_hover`, `_pressed` or `_disabled` are optional: without them the normal image\n"
         << "is brightened or darkened automatically.\n\n"
         << "| File | Size | 9-slice | What it is |\n|---|---|---|---|\n";

    for (const auto& info : S().catalog) {
        list << "| `" << info.name << ".png` | " << info.width << " x " << info.height << " | "
             << (info.nineSlice ? "yes" : "") << " | " << info.description << " |\n";

        const DrawFn& fn = info.templateDraw ? info.templateDraw : info.fallback;
        if (!fn) continue;
        SDL_Texture* target = SDL_CreateTexture(r, SDL_PIXELFORMAT_RGBA32, SDL_TEXTUREACCESS_TARGET, info.width,
                                                info.height);
        if (!target) continue;
        SDL_SetTextureBlendMode(target, SDL_BLENDMODE_BLEND);
        SDL_SetRenderTarget(r, target);
        SDL_SetRenderDrawBlendMode(r, SDL_BLENDMODE_NONE);
        SDL_SetRenderDrawColor(r, 0, 0, 0, 0);
        SDL_RenderClear(r);
        SDL_SetRenderDrawBlendMode(r, SDL_BLENDMODE_BLEND);
        fn(r, SDL_FRect{0, 0, static_cast<float>(info.width), static_cast<float>(info.height)});

        SDL_Surface* shot = SDL_RenderReadPixels(r, nullptr);
        SDL_Surface* rgba = shot ? SDL_ConvertSurface(shot, SDL_PIXELFORMAT_RGBA32) : nullptr;
        if (rgba) {
            std::string path = folder + info.name + ".png";
            std::string dir = path.substr(0, path.find_last_of('/'));
            SDL_CreateDirectory(dir.c_str());
            if (stbi_write_png(path.c_str(), rgba->w, rgba->h, 4, rgba->pixels, rgba->pitch)) ++written;
            else SDL_Log("Art: couldn't write %s", path.c_str());
        }
        if (rgba) SDL_DestroySurface(rgba);
        if (shot) SDL_DestroySurface(shot);
        SDL_SetRenderTarget(r, oldTarget);
        SDL_DestroyTexture(target);
    }

    std::string listPath = folder + "ART_LIST.md";
    std::string text = list.str();
    SDL_SaveFile(listPath.c_str(), text.data(), text.size());
    return written;
}

} // namespace art

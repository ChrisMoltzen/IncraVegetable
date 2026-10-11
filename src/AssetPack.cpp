#include "AssetPack.h"

#include "AssetPackFormat.h"

#include <SDL3/SDL.h>
#include <cstring>

#ifdef INCRA_PACKED_ASSETS
// Made by tools/AssetPacker (build/release/AssetPack.cpp).
extern const char kIncraPackData[];
extern const AssetPackEntry kIncraPackEntries[];
extern const std::size_t kIncraPackCount;
#endif

namespace assetpack {

namespace {
bool inPack(const std::string& path) { return path.rfind(kPrefix, 0) == 0; }

const AssetPackEntry* find(const std::string& path) {
#ifdef INCRA_PACKED_ASSETS
    const char* name = path.c_str() + std::strlen(kPrefix);
    for (std::size_t i = 0; i < kIncraPackCount; ++i)
        if (std::strcmp(kIncraPackEntries[i].name, name) == 0) return &kIncraPackEntries[i];
#else
    (void)path;
#endif
    return nullptr;
}
} // namespace

bool embedded() {
#ifdef INCRA_PACKED_ASSETS
    return true;
#else
    return false;
#endif
}

bool exists(const std::string& path) {
    if (inPack(path)) return find(path) != nullptr;
    SDL_PathInfo info;
    return SDL_GetPathInfo(path.c_str(), &info) && info.type == SDL_PATHTYPE_FILE;
}

void* load(const std::string& path, std::size_t* size) {
    if (!inPack(path)) return SDL_LoadFile(path.c_str(), size);
#ifdef INCRA_PACKED_ASSETS
    const AssetPackEntry* e = find(path);
    if (!e) return nullptr;
    auto* out = static_cast<unsigned char*>(SDL_malloc(e->size + 1));
    if (!out) return nullptr;
    std::memcpy(out, kIncraPackData + e->offset, e->size);
    scramble(out, e->size, e->offset);
    out[e->size] = 0; // like SDL_LoadFile, so text files are null-terminated
    if (size) *size = e->size;
    return out;
#else
    return nullptr;
#endif
}

std::vector<std::string> list(const std::string& folder, const std::string& extension) {
    std::vector<std::string> out;
    [[maybe_unused]] auto ends = [&extension](const std::string& n) {
        return n.size() > extension.size() && n.compare(n.size() - extension.size(), extension.size(), extension) == 0;
    };
    if (inPack(folder)) {
#ifdef INCRA_PACKED_ASSETS
        std::string want = folder.substr(std::strlen(kPrefix));
        if (!want.empty() && want.back() != '/') want += '/';
        for (std::size_t i = 0; i < kIncraPackCount; ++i) {
            const std::string name = kIncraPackEntries[i].name;
            if (name.rfind(want, 0) == 0 && name.find('/', want.size()) == std::string::npos && ends(name))
                out.push_back(name.substr(want.size()));
        }
#endif
        return out;
    }
    int count = 0;
    char** files = SDL_GlobDirectory(folder.c_str(), ("*" + extension).c_str(), 0, &count);
    if (files) {
        for (int i = 0; i < count; ++i)
            if (!std::strchr(files[i], '/')) out.push_back(files[i]);
        SDL_free(files);
    }
    return out;
}

} // namespace assetpack

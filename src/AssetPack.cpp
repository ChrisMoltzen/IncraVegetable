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

} // namespace assetpack

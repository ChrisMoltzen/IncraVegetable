// AssetPackFormat.h - shared by the game and tools/AssetPacker.
//
// A release build has every file from assets/ compiled into the executable
// (see AssetPack.h). The bytes are scrambled so the art and sounds can't be
// pulled out with a casual look at the .exe. It isn't encryption: someone
// determined, with a debugger, can still get at them.
#pragma once

#include <cstddef>
#include <cstdint>

struct AssetPackEntry {
    const char* name;   // path inside assets/, with forward slashes, e.g. "ui/coin.png"
    std::size_t offset; // into the data
    std::size_t size;
};

namespace assetpack {

// The scramble is its own inverse: run it again to get the original bytes back.
// `start` is the byte's offset in the whole pack, so any part can be unscrambled on its own.
inline void scramble(unsigned char* bytes, std::size_t count, std::size_t start) {
    for (std::size_t i = 0; i < count; ++i) {
        std::uint64_t x = (start + i) * 0x9E3779B97F4A7C15ull + 0x1F2E3D4C5B6A7988ull;
        x ^= x >> 31;
        x *= 0xBF58476D1CE4E5B9ull;
        x ^= x >> 29;
        bytes[i] ^= static_cast<unsigned char>(x >> 24);
    }
}

} // namespace assetpack

// AssetPack.h - reading files from assets/, or from the copy compiled into a
// release build.
//
// Development builds read assets/ from disk (so F5 picks up your edits).
// Release builds (make release / make release-win) have every asset compiled
// in, and art::assetDir() is then "pack://": paths under it are read from the
// executable, and nothing on disk is looked at, so players can't swap files.
#pragma once

#include <cstddef>
#include <string>
#include <vector>

namespace assetpack {

inline constexpr const char* kPrefix = "pack://";

bool embedded();                    // true in a release build with the assets compiled in
bool exists(const std::string& path); // a file (on disk, or "pack://..." in the pack)
// The whole file, or nullptr. Free it with SDL_free.
void* load(const std::string& path, std::size_t* size);
// The names of the files in a folder (on disk, or "pack://..."), e.g. list(dir + "lang", ".txt").
std::vector<std::string> list(const std::string& folder, const std::string& extension);

} // namespace assetpack

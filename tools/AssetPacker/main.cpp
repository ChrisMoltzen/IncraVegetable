// AssetPacker - turns assets/ into a C++ file for a release build.
//
//   AssetPacker <assets folder> <output .cpp>
//
// Every art, sound and settings file is packed into one scrambled block of
// bytes (see AssetPackFormat.h) with a table of names. `make release` runs
// this and compiles the result into the game.
#include "AssetPackFormat.h"

#include <algorithm>
#include <cstdio>
#include <filesystem>
#include <fstream>
#include <iterator>
#include <string>
#include <vector>

namespace fs = std::filesystem;

int main(int argc, char** argv) {
    if (argc != 3) {
        std::fprintf(stderr, "usage: AssetPacker <assets folder> <output .cpp>\n");
        return 1;
    }
    const fs::path root = argv[1];
    if (!fs::is_directory(root)) {
        std::fprintf(stderr, "AssetPacker: no folder %s\n", argv[1]);
        return 1;
    }
    const std::vector<std::string> kinds = {".png", ".jpg", ".jpeg", ".bmp", ".tga", ".txt", ".wav", ".ogg", ".mp3"};
    struct File {
        std::string name;
        std::vector<unsigned char> bytes;
    };
    std::vector<File> files;
    for (const auto& entry : fs::recursive_directory_iterator(root)) {
        if (!entry.is_regular_file()) continue;
        std::string ext = entry.path().extension().string();
        std::transform(ext.begin(), ext.end(), ext.begin(), [](unsigned char c) { return static_cast<char>(std::tolower(c)); });
        if (std::find(kinds.begin(), kinds.end(), ext) == kinds.end()) continue; // e.g. README.md
        std::ifstream in(entry.path(), std::ios::binary);
        File f;
        f.name = fs::relative(entry.path(), root).generic_string();
        f.bytes.assign(std::istreambuf_iterator<char>(in), std::istreambuf_iterator<char>());
        files.push_back(std::move(f));
    }
    std::sort(files.begin(), files.end(), [](const File& a, const File& b) { return a.name < b.name; });

    std::ofstream out(argv[2], std::ios::binary);
    if (!out) {
        std::fprintf(stderr, "AssetPacker: can't write %s\n", argv[2]);
        return 1;
    }
    out << "// Made by tools/AssetPacker from " << root.generic_string() << " - don't edit.\n"
        << "#include \"AssetPackFormat.h\"\n\n";
    // The data: one long string literal, written in octal escapes (which never run into the next byte).
    out << "extern const char kIncraPackData[];\nconst char kIncraPackData[] =\n";
    std::size_t offset = 0, total = 0;
    for (const auto& f : files) total += f.bytes.size();
    std::vector<std::size_t> offsets;
    std::string line;
    char esc[8];
    for (auto& f : files) {
        offsets.push_back(offset);
        std::vector<unsigned char> b = f.bytes;
        assetpack::scramble(b.data(), b.size(), offset);
        for (unsigned char c : b) {
            std::snprintf(esc, sizeof esc, "\\%03o", c);
            line += esc;
            if (line.size() >= 4000) {
                out << "    \"" << line << "\"\n";
                line.clear();
            }
        }
        offset += b.size();
    }
    out << "    \"" << line << "\";\n\n";
    out << "extern const AssetPackEntry kIncraPackEntries[];\nconst AssetPackEntry kIncraPackEntries[] = {\n";
    for (std::size_t i = 0; i < files.size(); ++i)
        out << "    {\"" << files[i].name << "\", " << offsets[i] << "u, " << files[i].bytes.size() << "u},\n";
    if (files.empty()) out << "    {\"\", 0u, 0u},\n";
    out << "};\n";
    out << "extern const std::size_t kIncraPackCount;\nconst std::size_t kIncraPackCount = " << files.size() << ";\n";
    std::printf("AssetPacker: %zu files, %.1f KB, from %s\n", files.size(), total / 1024.0, root.generic_string().c_str());
    return 0;
}

#!/bin/sh
# Generates an Xcode project for iPhone/iPad in build-ios/ and opens it.
# Usage:  ./build-ios.sh YOUR_TEAM_ID com.yourname.incravegetable [more CMake options]
# (Team ID: Xcode > Settings > Accounts, or developer.apple.com > Membership.)
# Extra options go straight to CMake, e.g.
#   ./build-ios.sh ABCDE12345 com.me.incravegetable -DINCRA_RELEASE=ON
#   ./build-ios.sh ABCDE12345 com.me.incravegetable -DINCRA_DEMO=ON
set -e
cd "$(dirname "$0")"
if ! command -v cmake >/dev/null 2>&1; then
    echo "CMake isn't installed. Install it with:  brew install cmake" >&2
    exit 1
fi
if ! xcode-select -p >/dev/null 2>&1; then
    echo "Xcode isn't set up. Install Xcode, open it once, then run:  sudo xcode-select -s /Applications/Xcode.app" >&2
    exit 1
fi
TEAM="${1:-}"
BUNDLE="${2:-com.example.incravegetable}"
[ $# -gt 0 ] && shift
[ $# -gt 0 ] && shift
cmake -S . -B build-ios -G Xcode \
    -DCMAKE_SYSTEM_NAME=iOS \
    -DCMAKE_OSX_DEPLOYMENT_TARGET=15.0 \
    -DINCRA_DEVELOPMENT_TEAM="$TEAM" \
    -DINCRA_BUNDLE_ID="$BUNDLE" \
    "$@"
open build-ios/IncraVegetable.xcodeproj

#!/bin/sh
# Generates an Xcode project for iPhone/iPad in build-ios/ and opens it.
# Usage:  ./build-ios.sh YOUR_TEAM_ID com.yourname.incravegetable
# (Team ID: Xcode > Settings > Accounts, or developer.apple.com > Membership.)
set -e
TEAM="${1:-}"
BUNDLE="${2:-com.example.incravegetable}"
cmake -S . -B build-ios -G Xcode \
    -DCMAKE_SYSTEM_NAME=iOS \
    -DCMAKE_OSX_DEPLOYMENT_TARGET=15.0 \
    -DINCRA_DEVELOPMENT_TEAM="$TEAM" \
    -DINCRA_BUNDLE_ID="$BUNDLE"
open build-ios/IncraVegetable.xcodeproj

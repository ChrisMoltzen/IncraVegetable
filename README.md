# IncraVegetable

An incremental farming game written in C++20 with SDL3. Runs on Windows, macOS, Linux and iPhone/iPad.

Vegetables grow in a small patch in the middle of the screen. Hover the mouse over a ripe one (or hold your finger on it) to pick it. When the sun sets, spend your coins in the tech tree, then start the next day.

The game ships with built-in art drawn in code, and every picture can be swapped for your own artwork (see **Artwork** below). The music and sound effects are synthesised in code.

## Building on desktop

You need CMake 3.21+ and a C++20 compiler. If SDL3 is installed CMake uses it; if not, CMake downloads and builds SDL3 for you the first time (this takes a few minutes).

**Windows (Visual Studio 2022)**

```
cmake -S . -B build
cmake --build build --config Release
build\Release\IncraVegetable.exe
```

Or open the folder in Visual Studio ("Open a local folder") and it picks up `CMakeLists.txt` on its own. `SDL3.dll` is copied next to the .exe after each build.

**macOS / Linux**

```
cmake -S . -B build
cmake --build build -j
./build/IncraVegetable
```

On Linux, if SDL3 has to be built from source you'll need the usual X11/Wayland and audio (ALSA/PulseAudio/PipeWire) development packages for your distro.

## Building for iPhone and iPad

You need a Mac with Xcode 15 or newer and CMake 3.21+ (`brew install cmake`).

1. Find your Apple **Team ID**: in Xcode open *Settings > Accounts*, select your Apple ID and team (a free personal team works for running on your own device).
2. Pick a unique **bundle identifier**, e.g. `com.yourname.incravegetable`.
3. From the project folder run:

   ```
   ./build-ios.sh YOUR_TEAM_ID com.yourname.incravegetable
   ```

   This generates `build-ios/IncraVegetable.xcodeproj` (downloading and building SDL3 for iOS) and opens it.
4. In Xcode choose the **IncraVegetable** scheme, pick your iPhone/iPad or a simulator, and press Run.

The first time you run on a real device, iOS may ask you to trust the developer certificate under *Settings > General > VPN & Device Management*.

iOS details already handled:
- Landscape only, full screen, status bar hidden, works on iPhone and iPad.
- The app icon is in `platform/ios/Assets.xcassets` (replace `AppIcon-1024.png` with your own 1024x1024 PNG).
- Touch controls: hold a finger on vegetables and drag across the patch to pick. In the tech tree, tap an upgrade to see what it does, then tap it again to buy it. Drag to scroll.
- The game pauses and saves when you switch apps or get a phone call, and saves if iOS closes it.
- Resolution/fullscreen settings and the Quit buttons are hidden on mobile (the OS controls those).

## Menus

- **Continue** - jumps back into the last farm you played, exactly where you left it (even mid-day).
- **New Game** - pick one of 3 save slots. If the slot already has a farm you're asked before it's overwritten.
- **Load Game** - pick any saved farm.
- **Settings** - Music volume, Sound FX volume, and on desktop the window resolution and fullscreen.
- **Quit** (desktop only).

During play, the pause button in the top-right corner (or Esc) opens the pause menu: Resume, Settings, Main Menu, and Quit Game on desktop.

## Artwork

Every picture in the game can be replaced with your own images, one piece at a time. Anything you haven't drawn yet keeps using the built-in art, so the game always works.

1. Open **`art_templates/`**. It has every image the game uses, already drawn at the right size and with the right name, and **`ART_LIST.md`** says what each one is for.
2. Copy the ones you want to change into **`assets/`** (keep the same sub-folders, e.g. `assets/crops/carrot.png`) and paint over them. PNG with transparency is best; JPG, BMP and TGA work too.
3. Run the game. On desktop, press **F5** while it's running to reload your art instantly. No rebuild needed.

What you can replace:

- **Crops**: ripe lettuce, carrot and pumpkin, a growing sprout (one shared, or one per crop)
- **Farm**: background, the garden bed, the soil mound under each plant (and under the pointer), top bar
- **Effects**: pick particles, the Wide Reach circle
- **HUD**: logo, coin, day timer bar, picking bar, pause and debug buttons
- **Menus**: main menu background and logo, save-slot screen background, pop-up panel, summary header, volume sliders
- **Buttons**: four colours, each with optional hover / pressed / disabled versions
- **Tech tree**: background, top bar, upgrade boxes (four states), tooltip, and an icon for each upgrade
- **Font**: an optional bitmap font that replaces the built-in pixel font

Things to know:

- Your images don't have to match the template size; they're scaled to fit. Keep the same shape (proportions) for best results.
- Buttons, panels, upgrade boxes and the tooltip are **9-slice**: their corners keep their shape and the middle stretches, so one image fits every size. You can set the corner size in `assets/art.txt`.
- Making pixel art? Put `filter nearest` in `assets/art.txt` so it stays crisp.
- Draw `fx/particle` and `ui/font` in white; the game colours them.
- If a file's name doesn't match anything (a typo), the game ignores it and the debug screen's Info tab (F1) lists it.
- Changed something and want fresh templates? Run `IncraVegetable --export-art-templates` (optionally followed by a folder name) to rewrite them.
- The `assets/` folder is copied next to the executable when you build, and packed into the app on iOS.
- The debug screen itself always uses the built-in look.

## Debug screen

Press **F1** (or tap the purple bug button in the bottom-left corner of any screen) to open it. The game pauses while it's open. Everything takes effect immediately, including on a day that's in progress.

| Tab | What you can do |
|---|---|
| **Money** | Quick +1 … +1T buttons, -100/-1K, x10, set coins to 0, "Afford everything" (enough to buy the whole tech tree), or type any amount (`2500`, `2.5K`, `3M`, `1B`, `4T`, `1e15`) and **Set** or **Add** it |
| **Time** | Change the day length (±1s/10s/60s, or type it; "Use upgrades" goes back to the tech tree value), add time to today, refill or end today, freeze the timer, game speed 0.25x–10x, change the day number |
| **Farm** | Ripen every crop, restart today, instant grow, instant pick, plant info overlay (growth %, pick %, crop, hit circles), FPS counter |
| **Tech** | Set any upgrade's level (0 / − / + / Max), max or reset the whole tree |
| **Info** | FPS, current screen, slot, every stat in effect, platform, save folder, Save now, Reload last save |

Debug overrides (day length, speed, instant grow/pick, freeze) only last until you quit; they're never saved. Coins, day number and upgrade levels you change *are* saved like normal progress.

To leave the debug screen and its button out of a release build:

```
cmake -S . -B build -DINCRA_DEBUG_TOOLS=OFF
```

## Saving

The game saves automatically - you never lose more than a few seconds:
- every 15 seconds while picking, and at sunset
- after every tech tree purchase and when a new day starts
- when you pause, go to the main menu, quit, or (on phones) switch away from the app

Saves are written to a temporary file and then swapped in, and the previous save is kept as a `.bak`, so a crash or power cut can't wipe your farm. If a save is ever damaged the backup is loaded instead. Saves from version 0.1 (`save.txt`) are moved into Slot 1 automatically.

Save and settings files live in your user data folder:

- Windows: `%APPDATA%\IncraVegetable\IncraVegetable\`
- macOS: `~/Library/Application Support/IncraVegetable/IncraVegetable/`
- Linux: `~/.local/share/IncraVegetable/IncraVegetable/`
- iOS: inside the app's own storage (removed if the app is deleted)

## Controls

| Where | Mouse / keyboard | Touch |
|---|---|---|
| Farm | Hover over a ripe vegetable until its bar fills | Hold a finger on it (drag across the patch) |
| Day summary | Click **Tech Tree** (or Space / Enter) | Tap **Tech Tree** |
| Tech tree | Click to buy, drag to pan, Home to recentre, **Start Day** (or Space / Enter) | Tap to inspect, tap again to buy, drag to pan |
| Anywhere in play | Esc or the pause button | Pause button |
| Anywhere | F1 opens the debug screen | Bug button, bottom-left |

## The tech tree

| Upgrade | Effect | Requires |
|---|---|---|
| Bigger Patch | Room for more vegetables (9 up to 100) | — |
| Longer Days | +5 seconds per day | — |
| Quick Hands | Pick 18% faster per level | — |
| Head Start | 20% of the patch is ripe at dawn per level | Longer Days 2 |
| Fertile Soil | Grow 12% faster per level | Bigger Patch 1 |
| Prize Produce | +25% coins per level | Quick Hands 1 |
| Wide Reach | Pick everything in a circle around the pointer | Quick Hands 3 |
| Carrots | Plant carrots (4 coins) | Fertile Soil 2 |
| Pumpkins | Plant pumpkins (15 coins) | Carrots, Prize Produce 3 |

Nodes stay hidden until one of their prerequisites has been bought, so the tree reveals itself as you play.

### Adding a new upgrade

1. **`include/TechTree.h`** – if the upgrade changes a new number, add a field for it to `Stats` with its starting value.
2. **`src/TechTree.cpp`** – add one `addNode({...})` block in the `TechTree` constructor.
3. Use the new `Stats` field where it matters, usually in `src/Farm.cpp`.

The tech tree screen and saving pick up new nodes automatically, and old saves keep working.

## Code layout

```
IncraVegetable/
├── src/            all the .cpp files
├── include/        all the .h files (including the stb image headers)
├── assets/         your artwork
├── art_templates/  every image at the right size, ready to paint over
├── platform/ios/   iOS Info.plist and app icon
└── CMakeLists.txt
```

Each `Name.*` below is `src/Name.cpp` plus `include/Name.h`.

| File | What it does |
|---|---|
| `main.cpp` | (no header) SDL main callbacks (the entry point on every platform) |
| `Game.*` | Screen flow, HUD, autosaving, pause, display settings, app lifecycle |
| `Menus.*` | Main menu, save slot picker, settings and pause menus |
| `DebugMenu.*` | The F1 debug screen |
| `UI.*` | Buttons and panels shared by the menus (mouse and touch) |
| `SaveSystem.*` | Save slots, crash-safe writes, backups, settings file |
| `Audio.*` | Synthesised music and sound effects, volume control |
| `Farm.*` | The patch: growing, picking, crops, particles, mid-day save state |
| `TechTree.*` | Upgrade definitions, costs, prerequisites, `Stats` |
| `TechTreeScreen.*` | Tech tree UI: nodes, lines, tooltips, panning, buying |
| `Draw.*` | Shape and text helpers (and the custom font) |
| `Art.*` | Loads your images from `assets/`, falls back to built-in art, F5 reload, template export |
| `ArtCatalog.cpp` | (no header) The list of every art slot: name, size, description and built-in drawing |
| `stb_image.h`, `stb_image_write.h` | Third-party (public domain) PNG reading and writing, in `include/` |
| `Platform.h` | (header only) Desktop vs mobile switches |

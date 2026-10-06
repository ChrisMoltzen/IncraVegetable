# IncraVegetable

An incremental farming game written in C++20 with SDL3. Runs on Windows, macOS, Linux and iPhone/iPad.

Vegetables grow in a small patch in the middle of the screen. Hover the mouse over a ripe one (or hold your finger on it) to pick it. When the sun sets, spend your coins in the tech tree, then start the next day.

The game ships with built-in art drawn in code, and every picture can be swapped for your own artwork (see **Artwork** below). The music and sound effects are synthesised in code.

## Building on desktop

The game builds with the `Makefile` and needs a C++20 compiler and SDL3. Everything goes into `build/`.

**macOS** (clang, with SDL3.framework in `/Library/Frameworks`)

```
make                # the game: build/game
make editor         # the Tech Tree Editor: build/TechTreeEditor
```

**Windows** (64-bit MinGW g++ from MSYS2 UCRT64, with the SDL3 *VC* download unzipped to `C:\SDL`, so `C:\SDL\include` and `C:\SDL\lib\x64` exist)

```
make win            # the game: build\game.exe
make editor-win     # the Tech Tree Editor: build\TechTreeEditor.exe
```

- Both copy the 64-bit `SDL3.dll` into `build\` for you, so the .exe runs straight away.
- The C++ runtime is built into the .exe, so it doesn't need MSYS2's DLLs: you can run it from any Command Prompt, by double-clicking, or on another PC (with `SDL3.dll` next to it).
- SDL somewhere else? `make win SDL_DIR=D:/libs/SDL3`.
- `g++ --version` must be 10 or newer (for C++20).
- Error **0xc000007b** when starting means a 32-bit DLL got loaded. Make sure `build\SDL3.dll` is the one from `lib\x64`, not `lib\x86` (`make win` copies the right one).

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
- **Farmers**: `farm/farmer` (standing), plus optional `farm/farmer_walk1` / `farm/farmer_walk2` (walking steps) and `farm/farmer_pick` (bending to pick). Only `farm/farmer` is needed: missing poses use it, bobbing as it walks
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
- Draw the farmer **facing right** with its feet at the bottom middle (about 8% up from the bottom edge). The game mirrors it when a farmer walks left.
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
| More Seeds | About 25% more crops growing at once per level (4 up to 114, capped by the patch's room) | — |
| Longer Days | +5 seconds per day | — |
| Quick Hands | Pick 18% faster per level | — |
| Head Start | 20% of the patch is ripe at dawn per level | Longer Days 2 |
| Fertile Soil | Grow 12% faster per level | Bigger Patch 1 |
| Prize Produce | +25% coins per level | Quick Hands 1 |
| Wide Reach | Pick everything in a circle around the pointer | Quick Hands 3 |
| Carrots | Plant carrots (4 coins) | Fertile Soil 2 |
| Pumpkins | Plant pumpkins (15 coins) | Carrots, Prize Produce 3 |
| Helping Hand | Unlocks auto-pick: picking a crop has a 10% chance to also pick 1 ripe crop within 1.5 plant widths | Wide Reach 1 |
| Lucky Streak | +5% auto-pick chance per level (up to 60%) | Helping Hand |
| Bumper Bunch | Auto-pick picks 1 more crop per level (up to 6) | Helping Hand |
| Spreading Roots | Auto-pick reaches +0.5 plant widths per level (up to 4.5) | Helping Hand |
| Farmhand | Hires a farmer who walks the patch picking ripe crops | Fertile Soil 1 |
| Farm Crew | +1 farmer per level (up to 5) | Farmhand |
| Comfy Boots | Farmers walk 20% faster per level | Farmhand |
| Sharp Shears | Farmers pick 15% faster per level | Farmhand |

Nodes stay hidden until one of their prerequisites has been bought, so the tree reveals itself as you play.

**Auto-pick** (Helping Hand and its upgrades): every time you pick a crop yourself, the chance is rolled once. If it succeeds, the nearest ripe crops within the radius are picked too, up to the crop count, with a green sparkle trail and an "Auto-pick!" pop-up. Crops picked this way never set off another auto-pick. The debug screen's *Show plant info* draws the auto-pick range around the plant under the pointer.

**Farmers** (Farmhand and its upgrades): each farmer heads for the nearest ripe crop that no other farmer is going for, walks to it, picks it and moves on. With nothing ripe they wander about the bed. If you pick their crop first, they simply find another. Crops farmers pick don't set off auto-pick. They start each day at the front of the bed, and stop when the day ends.

### Designing the tech tree: TechTreeEditor

The tech tree is data, not code. It lives in **`include/TechTreeData.h`**, which is compiled into the game, so everything is inside the executable and there's no file to ship. You edit it with the **Tech Tree Editor**:

```
make editor
build/TechTreeEditor
```

On Windows: `make editor-win`, then `build\TechTreeEditor.exe`.

Run it from the project folder (or pass the path to `TechTreeData.h`). It opens the current tree.

- **Canvas (left):** the tree as it looks in the game.
  - Double-click empty space to add a tech.
  - Drag a tech to move it; positions snap to half steps.
  - Shift+click another tech to add or remove a requirement. The arrow points at the tech that needs it.
  - Right-drag (or drag empty space) to pan, mouse wheel to zoom, F to fit everything in view.
- **Panel (right):** everything about the selected tech.
  - **ID:** letters, numbers and `_` only. Don't rename a tech after players have saves, because saves store levels by id. If you rename one anyway, the techs that require it are updated for you.
  - **Name** and **Description:** shown in the game's tooltip.
  - **Max level**, **Cost** of level 1, and **Cost growth** (each level costs this many times the last).
  - **Position** in columns and rows.
  - **Requirements:** which techs, at which level. A tech's box stays hidden in the game until one of these has been bought.
  - **Effects:** click the first button to pick a stat from a list (each with a line saying what it does), the second to choose how it changes (Shift+click goes backwards), then type the amount. Each effect shows in words what it does at level 1 and at max level.
  - With nothing selected, the panel shows **With everything bought**: every stat at the start and with the whole tree bought. It also warns when *Crops at once* and the patch's room don't match up (crops can never be more than the patch has room for).
  - **Preview:** the cost of every level and what the tech gives at each level.
  - **Problems:** missing requirements, loops that make techs impossible to buy, duplicate ids, techs on the same spot, techs with no effects.
- **Keys:** Ctrl+S save, Ctrl+Z / Ctrl+Y undo / redo, Ctrl+D duplicate, Delete removes the selected tech, arrow keys nudge it. On a Mac, Cmd works too.

After saving, run `make` again to rebuild the game with the new tree.

**Effects.** Each effect changes one stat. For a tech at level L:

| Operation | What it does | Example |
|---|---|---|
| `+ per level` | stat + amount × L | Day length + 5 → 25, 30, 35 s... |
| `+% per level` | stat × (1 + amount% × L) | Coin value + 25 → ×1.25, ×1.5... |
| `x per level` | stat × amount^L | Pick time × 0.82 → 18% faster each level |
| `set at least` | stat = at least amount | Crops at least 1 → unlocks carrots |

The stats a tech can change:

| Stat | What it is |
|---|---|
| Patch size | Room in the bed (size × size plants) |
| Crops at once | Vegetables growing at the same time (starts at 4, never more than the patch's room) |
| Day length | Seconds per day |
| Pick time | Seconds to pick |
| Grow time | Seconds to grow |
| Coin value | Coin multiplier |
| Reach | Picking radius |
| Crops | 0 lettuce, 1 + carrots, 2 + pumpkins |
| Head start | Fraction ripe at dawn |
| Auto-pick chance | % chance that picking a crop sets off an auto-pick (0 at the start) |
| Auto-pick crops | How many of the nearest ripe crops an auto-pick picks (0 = off) |
| Auto-pick radius | How far away counts as nearby, in plant widths, centre to centre (0 = off) |
| Farmers | Helpers picking crops on their own (0 at the start) |
| Farmer speed | How fast farmers walk, in plant widths per second (starts at 2) |
| Farmer pick time | Seconds for a farmer to pick a lettuce; other crops take longer, as they do for you (starts at 1.5) |

**A brand-new kind of effect** (one that isn't a stat above) still needs a little code:

1. Add a field to `Stats` in `include/TechTree.h`.
2. Add it to the list in `src/TechData.cpp` (`stats()`, `getStat`, `setStat`, `formatStat`). After that, the editor offers it too.
3. Use it in the game, usually in `src/Farm.cpp`.

`TechTreeData.h` is readable text inside a C++ header, so you can also hand-edit it. The format is described at the top of `include/TechData.h`.

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
| `TechTree.*` | Tech tree rules: costs, prerequisites, buying, `Stats` |
| `TechData.*` | The tech tree file format, effects and checks (shared with the editor) |
| `TechTreeData.h` | (header only) The tech tree itself, written by the editor |
| `tools/TechTreeEditor/` | The Tech Tree Editor app (`make editor`) |
| `TechTreeScreen.*` | Tech tree UI: nodes, lines, tooltips, panning, buying |
| `Draw.*` | Shape and text helpers (and the custom font) |
| `Art.*` | Loads your images from `assets/`, falls back to built-in art, F5 reload, template export |
| `ArtCatalog.cpp` | (no header) The list of every art slot: name, size, description and built-in drawing |
| `stb_image.h`, `stb_image_write.h` | Third-party (public domain) PNG reading and writing, in `include/` |
| `Platform.h` | (header only) Desktop vs mobile switches |

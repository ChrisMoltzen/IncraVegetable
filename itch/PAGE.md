# IncraVegetable — itch.io page kit

Everything for the game's itch.io page: what goes in each box of the
**Edit game** form, the description to paste, and which image goes where.

---

## The form fields

| Field | Fill in |
|---|---|
| **Title** | IncraVegetable |
| **Project URL** | `yourname.itch.io/incravegetable` |
| **Short description or tagline** | Hover to harvest, grow your patch and unlock 166 upgrades in a cosy incremental farming game. |
| **Classification** | Games |
| **Kind of project** | Downloadable |
| **Release status** | In development (switch to Released when you're happy with it) |
| **Pricing** | Your call. *No payments* or *Donate* suits a first release; *Paid* with a low minimum also works. |
| **Genre** | Simulation |
| **Tags** (up to 10) | `incremental`, `idle`, `farming`, `clicker`, `pixel-art`, `casual`, `relaxing`, `cozy`, `short`, `singleplayer` |
| **AI generation disclosure** | Answer honestly for code, art and text. itch asks about each one separately. |
| **App store links** | Leave empty for now (there's an iPhone/iPad build in the repo if you put it on the App Store later). |
| **Custom noun** | Leave empty |
| **Community** | Comments |
| **Visibility** | Draft while you set it up, then Public |

**Uploads.** Mark each zip with its platform tick box:

| File | Platform |
|---|---|
| `IncraVegetable-windows.zip` | Windows |
| `IncraVegetable-mac.zip` | macOS |
| `IncraVegetable-demo-windows.zip` *(optional)* | Windows. Tick **This file is a demo** |
| `IncraVegetable-demo-mac.zip` *(optional)* | macOS. Tick **This file is a demo** |

If you sell the game, itch lets people download files marked as a demo for
free while the full game stays paid. The demo builds come from
`make demo-release` / `make demo-release-win`.

See **Before you upload** at the bottom: the Mac build needs one change first.

**Details → Average session:** About a half-hour. **Languages:** English.
**Inputs:** Mouse, Keyboard. **Accessibility:** "One button" doesn't quite
fit (you hover, not click), so leave those unticked.

---

## Images

| File | Where it goes |
|---|---|
| `cover_630x500.png` | **Cover image** (630 x 500, itch's recommended size; `cover_315x250.png` is the same at the minimum size) |
| `gameplay.gif` | **Cover GIF / first screenshot.** itch plays it when people hover the cover in browse pages if you set it as the *Cover animation*. It's also good at the top of the description. |
| `screenshots/*.png` | **Screenshots**, in number order (1280 x 720, taken from the release build: no debug button) |
| `banner_960x240.png` | **Theme → Banner**. The page column is 960 wide. |
| `page_background_tile.png` | **Theme → Background image**, set to *Repeat* |

**Suggested theme colours** (from the game's palette):

| Theme setting | Colour |
|---|---|
| Background | `#627F50` Leaf, the farm grass (only shows if you don't use the tile) |
| Background 2 (page panel) | `#2E3530` Panel |
| Text | `#F4EFE1` Cream |
| Link | `#D4B25E` Coin |
| Button | `#B8663A` Carrot (button text `#F4EFE1`) |
| Border / embed | `#1F2421` Night Soil |
| Font | A pixel font if you like (*Silkscreen* or *Press Start 2P*), with *Lato* for the body if the pixel one is too heavy for paragraphs |

---

## The description

Paste this into the **Description** box. It's written so the headings and
lists come through when pasted as formatted text; `description.html` has
the same thing as HTML if you'd rather use itch's HTML mode.

> *(Put `gameplay.gif` at the top of the description: in itch's editor, Insert image, then upload it.)*

**A tiny vegetable patch. A sun that sets far too soon. And a barn full of upgrades that turn a handful of lettuces into a field you can barely see the edges of.**

IncraVegetable is a cosy incremental farming game. Each day you get a few seconds of sunlight to pick whatever's ripe. Hover over a vegetable and it's yours. When the sun goes down, take your coins to The Barn and spend them on upgrades, then wake up to a bigger, faster, busier farm.

### How it plays

- **Hover to harvest.** No clicking, no frantic tapping: move the mouse over a ripe vegetable and hold it there for a moment. On a touchscreen, hold your finger on it.
- **Race the sunset.** The day/night dial in the top bar tells you how long you've got. When it's dark, the day's done.
- **Spend it all in The Barn.** 166 upgrades branch out from the barn like a snowflake. Every one is a single purchase, so there's always something just within reach.
- **Watch it grow.** Your patch goes from 2 x 2 to 30 x 30. Lettuces give way to carrots, pumpkins and turnips. Eventually you'll have help...

### Upgrades include

- **Bigger Patch** and **More Seeds**: more room, more vegetables growing at once
- **Longer Days**: a little more picking time every day
- **Quick Hands** and **Fertile Soil**: pick faster, grow faster
- **Carrots, Pumpkins and Turnips**: slower to grow, much more valuable
- **Prize Produce**: every vegetable sells for more
- **Wide Reach**: pick everything in a circle around the pointer
- **Head Start**: part of the patch is already ripe at dawn
- **Helping Hand**: picking a vegetable sometimes picks its neighbours too
- **Farmhands**: hire farmers who walk the patch and pick for you

### Also

- Three save slots, saved automatically (even mid-day)
- A stats page with time played, vegetables picked, best day and more
- Hand-made pixel art, swaying grass and a day that turns to dusk as you play
- About three hours to buy every upgrade
- *(If you upload the demo:)* **Try the free demo**: the first 55 upgrades, and your farm carries over to the full game

### Controls

| | |
|---|---|
| Pick | Hover over a ripe vegetable (touch: hold your finger on it) |
| The Barn | Hover to see an upgrade, click to buy. Drag to move, mouse wheel to zoom, **Home** to fit the whole tree |
| Stats | **Tab**, or the chart button in the top bar |
| Pause | **Esc**, or the pause button |

### Installing

**Windows:** unzip and run `IncraVegetable.exe`. Keep `SDL3.dll` in the same folder.
**macOS:** unzip and open the app. It isn't signed, so the first time, right-click it, choose **Open**, then **Open** again.

*Made with C++ and SDL3.*

---

## Before you upload

1. **Build the release versions.** `make release-win` makes `build\release\IncraVegetable.exe` and `SDL3.dll`. Zip the two files as `IncraVegetable-windows.zip`, and leave out `AssetPacker.exe` and `AssetPack.cpp`.
2. **The Mac build won't run on other people's Macs yet.** `make release` links SDL3 from `/Library/Frameworks`, which players won't have. It needs packaging as an `IncraVegetable.app` with `SDL3.framework` inside it (and the `.icns` from `icon/`). I can add a `make mac-app` target for that.
3. **Try the zips on a computer that's never had SDL or the source on it**, if you can.
4. **Version number.** The game says `v0.2` on the main menu. Bump it if this is a bigger release, and use the same number in the upload's display name.
5. **Demo (optional).** `make demo-release-win` / `make demo-release` build `IncraVegetable Demo.exe` (or `IncraVegetable Demo` on Mac) into `build/demo-release/`. Zip it with `SDL3.dll` the same way, as `IncraVegetable-demo-windows.zip`.
6. **Optional: butler.** itch's command-line uploader (`butler push`) makes updates painless and lets players' itch app patch instead of re-downloading.

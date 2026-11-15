# Steam upload files

`app_build.vdf` is the script `steamcmd` uses to upload the game. Before the first upload:

1. `app_build.vdf` already has the App ID (5631970) and Windows depot id (5631971); change them if you set up a new app.
2. `make steam-win STEAMWORKS=path/to/steamworks_sdk` stages the game in `build/steam-content/windows/`.
3. Upload it:
   `path/to/steamworks_sdk/tools/ContentBuilder/builder/steamcmd.exe +login YOUR_BUILD_ACCOUNT +run_app_build "%CD%\steam\app_build.vdf" +quit`
4. In Steamworks, under *SteamPipe > Builds*, set the build live on a branch, and set the **launch option** to `IncraVegetable.exe`.

The staged folder holds `IncraVegetable.exe`, `SDL3.dll` and `steam_api64.dll`, and no `steam_appid.txt` (Steam supplies the id).

For saves to follow players between PCs, turn on **Steam Auto-Cloud** (Steamworks > *Application > Steam Cloud*):
root `WinAppDataRoaming`, subdirectory `IncraVegetable/IncraVegetable`, pattern `*.sav` (the three slots and
`achievements.sav`). Leave out `settings.txt`: it holds the window size, which is per-PC.

## Store graphics

`steam/store_assets/` holds the capsule art, made from the game's own logo, sunset and crop sprites by
`python tools/SteamAssets/make_steam_assets.py` (needs `pip install pillow`; re-run it after changing the art).

| File | Steamworks slot |
| --- | --- |
| `header_capsule_920x430.png` | Store > Header capsule |
| `small_capsule_462x174.png` | Store > Small capsule |
| `main_capsule_1232x706.png` | Store > Main capsule |
| `vertical_capsule_748x896.png` | Store > Vertical capsule |
| `page_background_1438x810.png` | Store > Page background (optional) |
| `library_capsule_600x900.png` | Library > Capsule |
| `library_header_920x430.png` | Library > Header |
| `library_hero_3840x1240.png` | Library > Hero (no logo on it) |
| `library_logo_1280x720.png` | Library > Logo (transparent) |
| `community_icon_184x184.jpg` | Community icon |
| `client_icon_256x256.png`, `../../icon/IncraVegetable.ico` | Client icon |

Not generated: **screenshots** (at least 5 at 1920 x 1080: take them from the real game, e.g. the farm mid-harvest, the tech tree, the achievements page) and a **trailer**.

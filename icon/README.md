# Game icon

| File | Used for |
|---|---|
| `../assets/ui/app_icon.png` | The source art (32 x 32 pixel art). The game puts it on its window, the taskbar and the Mac Dock when it starts, scaled up crisply. It's packed into release builds like any other art. |
| `IncraVegetable.ico` | Windows: built into `IncraVegetable.exe` by `make win` / `make release-win` (via `IncraVegetable.rc` and `windres`), so the file itself shows the icon in Explorer. |
| `IncraVegetable.icns` | macOS: for a `.app` bundle (put it in `Contents/Resources/` and name it in `Info.plist` as `CFBundleIconFile`). |
| `IncraVegetable_1024.png` | Big copy for store pages (Steam, itch.io, App Store). |

If you repaint `app_icon.png`, the window icon updates straight away; the `.ico`, `.icns` and 1024 px copy
need making again from it (any icon tool will do: export 16, 24, 32, 48, 64, 128 and 256 px for the .ico,
scaling by whole numbers with "nearest neighbour" so the pixels stay sharp).

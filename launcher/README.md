# Northrend Launcher

An unofficial launcher for **Warmane's** Wrath of the Lich King realms, built with the Dear ImGui sources in this repo (Win32 + DirectX 9). It builds for **x86 and x64**.

> Not made by, affiliated with, or endorsed by Warmane or Blizzard Entertainment.

## What it does

1. **Connecting window**: checks that `logon.warmane.com:3724` accepts connections, then opens the launcher. If the check fails, you get **Retry** or **Continue offline**.
2. **Play**: pick a realm (Icecrown, Lordaeron, Onyxia, Blackrock) and press **Play**. The launcher then:
   - writes `set realmlist logon.warmane.com` to `Data\<locale>\realmlist.wtf`, clearing the file's read-only flag if needed;
   - sets `realmList` and `realmName` in `WTF\Config.wtf`, so the client goes straight to the realm you picked. Other lines are left alone;
   - optionally deletes the `Cache` folder and sets windowed or fullscreen mode;
   - starts `Wow.exe`, watches the process, and can minimize itself and come back when the game closes.
3. **Armory**: search any character on Warmane and view them in-app, laid out like the armory:
   - **Gear**: the 19-slot paper-doll. Each item shows its icon with a quality-colored frame, its name and slot, enchant and gem info, and a tooltip on hover. In the middle is a character panel with the race portrait in class colors, the spec, and the three weapon slots.
   - **Stats**: achievement points, honorable kills, equipped item count, professions and specs.

   Each search makes two requests to the armory: the public summary JSON (`/api/character/<name>/<realm>/summary`) and the character's profile page (`/character/<name>/<realm>/profile`), which has the gear layout. Results are cached for 5 minutes. Icons come from Warmane's icon CDN and are downloaded once, then cached in `%APPDATA%\NorthrendLauncher\icons`. It only reads public data.
4. **Addons**: lists `Interface\AddOns` (title, version, notes from each `.toc`), with search and an "Open folder" button.
5. **Settings**: game folder, realmlist, clear cache, display mode, what to do after launching, accent color, animations, snow.

It checks that `Wow.exe` is build **12340** (3.3.5a) and warns about other builds. If you put the launcher in your WoW folder, it finds the game automatically.

The launcher never reads or writes game memory and never asks for account details. You log in inside the game as usual.

Settings are saved in `%APPDATA%\NorthrendLauncher\settings.ini`.

## Building

### Visual Studio 2019 / 2022

Open `launcher/NorthrendLauncher.sln`, pick `Release` and `x86` or `x64`, then build. You need the **Desktop development with C++** workload.

| Platform | Output |
|----------|--------|
| x86 | `launcher/bin/x86/Release/NorthrendLauncher.exe` |
| x64 | `launcher/bin/x64/Release/NorthrendLauncher.exe` |

```bat
msbuild launcher\NorthrendLauncher.sln /p:Configuration=Release /p:Platform=x86
msbuild launcher\NorthrendLauncher.sln /p:Configuration=Release /p:Platform=x64
```

### CMake

```bat
cmake -S launcher -B build-x86 -A Win32 && cmake --build build-x86 --config Release
cmake -S launcher -B build-x64 -A x64   && cmake --build build-x64 --config Release
```

## Customizing

| What | Where |
|------|-------|
| Name, tagline, version, window sizes, icons | `src/theme.h` |
| Colors and accent presets | `src/theme.cpp` |
| Realm list (names, tags, descriptions), realmlist host, expected build | `src/game.cpp`, `src/game.h` |
| What Play writes and how the game starts | `Game::Launch()` in `src/game.cpp` |
| Logon server check | `src/logon_status.cpp` |
| Armory fetch, JSON and gear parsers | `src/armory.cpp`, `src/armory.h` |
| Icon download, disk cache, WIC decode | `src/icons.cpp` |
| HTTP helper (WinHTTP) | `src/net.cpp` |
| Pages (Play, Armory, Addons, Settings, About) | `src/pages.cpp` |
| Title bar, sidebar, status bar, toasts | `src/main_screen.cpp` |
| Connecting window | `src/connect_screen.cpp` |

The realm table reflects Warmane in October 2026. Update `Game::Realms` if realms are added or retired. Each name must match the in-game realm name, because it is written to `Config.wtf` as `realmName`.

## Armory API

Neither source is officially documented, so both parsers are tolerant:

- **Summary JSON** (`ParseCharacterSummary`): numbers may arrive as strings or `null`, and missing fields are hidden.
- **Profile page** (`ParseProfileGear`): reads the `.item-left` (8), `.item-right` (8) and `.item-bottom` (3) slot columns. Each filled slot carries `rel="item=…&ench=…&gems=…"`, an `icon-qualityN` class and an icon `<img>`.

If Warmane changes the page layout, the Gear tab falls back to a plain list of item names, and the parser needs a small update in `armory.cpp`.

`tests/armory_parse_test.cpp` covers both parsers and the icon decoder offline. To point the lookups at a different server (used for testing), set the `ARMORY_API_BASE` environment variable.

The character panel shows the race portrait rather than a 3D model. A real 3D "toon" would need a WoW model renderer that reads the game's MPQ archives; that is a separate, much larger project.

## Notes

- If the game is under `Program Files`, Windows may block writes to `realmlist.wtf`. Move the game folder, or run the launcher as administrator.
- Icons come from *Segoe Fluent Icons* / *Segoe MDL2 Assets* (Windows 10/11). Without them a small diamond is drawn instead.
- The launcher is per-monitor DPI aware. On Windows 11 it gets rounded corners.

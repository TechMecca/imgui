# ImGuiApp

A borderless Windows app built on the Dear ImGui sources in this repo (Win32 + DirectX 9), for **x86 and x64**.

1. **Connecting window**: a compact splash with a spinner, the stage name, and a progress bar while it connects to the server.
2. **Main window**: once the connection is verified, the window fades out, resizes, and opens the main UI. It has a sidebar and Dashboard, Modules, Settings and About pages.

There is **no server code**. The demo "connection" just waits 10 seconds (see [Connecting to your server](#connecting-to-your-server)).

## Building

### Visual Studio 2019 / 2022

1. Install the **Desktop development with C++** workload. It includes the Windows SDK, which has `d3d9.lib`, so you don't need the old DirectX SDK.
2. Open `app/ImGuiApp.sln`.
3. In the toolbar, pick a configuration (`Release` / `Debug`) and a platform (`x86` / `x64`), then **Build → Build Solution**.

| Platform | Output |
|----------|--------|
| x86 | `app/bin/x86/Release/ImGuiApp.exe` |
| x64 | `app/bin/x64/Release/ImGuiApp.exe` |

From a Developer Command Prompt:

```bat
msbuild app\ImGuiApp.sln /p:Configuration=Release /p:Platform=x86
msbuild app\ImGuiApp.sln /p:Configuration=Release /p:Platform=x64
```

Release builds link the C runtime statically, so the `.exe` runs without the VC++ redistributable.

### CMake

```bat
cmake -S app -B build-x86 -A Win32 && cmake --build build-x86 --config Release
cmake -S app -B build-x64 -A x64   && cmake --build build-x64 --config Release
```

MinGW-w64 works too (`i686-w64-mingw32-g++` / `x86_64-w64-mingw32-g++`).

## Connecting to your server

Everything lives in `src/connection.cpp`, in `ServerConnection::Run()`. It runs on a worker thread, so blocking calls are fine there and the UI keeps animating.

The demo goes through four stages (`Resolving`, `Handshake`, `Authenticating`, `Verifying`) and takes 10 seconds in total. To hook up a real server:

- Replace each `Wait(...)` with the real work for that step. Keep calling `SetStage()` and setting `m_Progress` as you go.
- On error, call `Fail("message")` and return. The connecting window shows the message with **Retry** / **Exit** buttons. To preview that screen, run `ImGuiApp.exe --demo-fail`.
- On success, set the stage to `ConnectStage::Verified`. The UI plays the check-mark animation, then opens the main window.
- If the process exits mid-connection, `Cancel()` sets `m_CancelRequested`. Check it in long loops.

## Customizing

| What | Where |
|------|-------|
| App name, tagline, version, window sizes | `src/theme.h` |
| Colors, accent presets, fonts | `src/theme.cpp` |
| Connecting window | `src/connect_screen.cpp` |
| Main window and pages | `src/main_screen.cpp` |
| Custom widgets (spinner, toggle, nav item, cards, charts...) | `src/widgets.cpp` |
| Win32 window, D3D9 device, main loop | `src/main.cpp` |

## Notes

- Icons come from *Segoe Fluent Icons* (Windows 11) or *Segoe MDL2 Assets* (Windows 10). If neither is installed, a small diamond is drawn instead.
- On Windows 11 the window gets rounded corners and a themed border from DWM. On Windows 10 it is square with a 1px border.
- The app is per-monitor DPI aware: fonts and layout are rebuilt when the window moves to a monitor with a different scale.
- Drag the window by its title bar, or anywhere on the connecting window.

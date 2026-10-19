# Caecilia Notch Island

Native C++20 Windows overlay: D3D11, DXGI, DirectComposition, Direct2D 1.1 and
DirectWrite. No web UI or outgoing network requests.

**M0 accepted by the user; M1 implemented and awaiting visual acceptance.**
M2–M4 have not started. See [M0](docs/m0-status.md), [M1](docs/m1-status.md),
and [performance](docs/perf.md). MSVC verification remains pending.

## Run without Visual Studio

The local portable build is at:

```text
artifacts\M1-portable\CaeciliaNotchIsland.exe
```

Run `Jalankan Demo.bat` in that folder, or:

```powershell
.\artifacts\M1-portable\CaeciliaNotchIsland.exe --demo --show-debug-fps
```

This is an unsigned alternative **MinGW x64** build with compiler runtime linked
statically. Windows system DLLs remain required. No Visual Studio installation
is needed to run it. Artifacts are ignored by Git, not published/downloadable releases.

Rebuild offline using the installed MinGW toolchain:

```powershell
powershell -NoProfile -ExecutionPolicy Bypass -File tools/build-portable.ps1
```

## Supported MSVC build

Provision VS 2022 Desktop development with C++, Windows SDK and CMake 3.25+ locally.
No build step downloads packages.

```powershell
cmake -S . -B build -G "Visual Studio 17 2022" -A x64
cmake --build build --config Debug
ctest --test-dir build -C Debug --output-on-failure
cmake --build build --config Release
.\build\Release\CaeciliaNotchIsland.exe --demo --debug-bounds --show-debug-fps
```

CMake keeps MSVC as the supported toolchain; app and tests use `/W4 /WX`.
M0/M1 use Windows SDK COM interfaces. C++/WinRT media starts in M2; an offline
include directory can be supplied with `-DCAECILIA_CPPWINRT_INCLUDE_DIR=...`.

## M1 demo controls

**Place the cursor inside the island before pressing demo keys.** Raw input does
not capture focus or suppress typing: keys still reach the foreground application.
No keys are logged, saved or transmitted. Demo input is only registered with `--demo`.

| Input | Action |
|---|---|
| `1` | Dormant, cancel HUD |
| `2` | Compact with active waveform, cancel HUD |
| `3` | Expanded, cancel HUD |
| `4` | Pin Volume HUD until another state selection |
| `V` / `B` / `P` | Volume / Brightness / Pomodoro HUD for 1800 ms; refresh on each press |
| Hover | Expand after 120 ms; collapse 250 ms after leaving |
| Click Dormant/Compact | Expand immediately |
| Click tab | Change tab with sprung pill and content fade |
| Ctrl+Alt+F12 | Toggle all implemented layout bounding boxes |
| Ctrl+Shift+F | Toggle active FPS / settled `IDLE` |

The HUD has priority over hover. A timed HUD returns to the current base/hover
state. `V/B/P` replace a pinned HUD with a timed one. Key auto-repeat does not
retrigger. Demo state selection consumes pending hover entry so `1`/`2` remain
visible until you leave/re-enter. Ctrl/Alt/Shift combinations are not demo inputs.

Data is **fictional and static** in M1: media controls, seeking, real files,
telemetry polling and system volume/brightness actions arrive in later milestones.
The clock samples local time when rendering; it does not have a periodic M1 timer.

With Windows animation effects off, shapes/tab positions snap, waves stop at the
minimum height, and both fades take 100 ms with no delay or blur.

## Options and shutdown

- `--demo`: enable the keys above; start Dormant.
- `--demo-media-active`: with `--demo`, show active Dormant dot.
- `--debug-bounds`, `--show-debug-fps`: start with the respective debug overlay.
- `--config <path>`: read an explicitly selected config containing `showDebugFps=true`
  or `showDebugFps=false`; command-line flags override it.
- `--verify-device-recovery`: tear down GPU resources after the first Present and
  recreate them. This is **not** a real GPU driver-failure injection.

Only one instance runs. No tray/menu until M3. Close through Task Manager, or:

```powershell
Stop-Process -Name CaeciliaNotchIsland
```

Errors go to `OutputDebugString`; no focus-stealing error dialog is opened.

## Architecture and verification

- Fixed 492 × 193 DIP HWND at primary-monitor `rcMonitor` top center.
- Shared current geometry for draw/clipping/hit testing; raw mouse input changes
  `WS_EX_TRANSPARENT` only at inside/outside transitions.
- Seven springs, semi-implicit Euler at 1/240 s, max elapsed 1/20 s per frame,
  velocity-preserving retargeting. Window is not resized during animation.
- Waitable DXGI frame-latency pacing while moving/waving. Blocking input/deadline
  wait when settled, with a final `IDLE` frame and no idle refresh timer.
- Masked rim, surface interpolation, shadow SPEED while animating/cached when
  settled, content fade/blur and grayscale DirectWrite text.
- Device-loss retries are bounded, not a busy render loop.

M0 visibility, debug controls and click-through/focus passed the user's tests.
M1 animation screenshots, rapid geometry-boundary clicks, DPI changes and GPU
performance still need validation on the target machine; no no-race guarantee
is claimed for asynchronous raw-input style changes.

Offline spec verification (does not open the HTML's CDN UI):

```powershell
node tools/verify-spec.cjs
```

`DESIGN.md` Appendix A equals `src/spec/NotchIslandSpec.h`; numeric visual values
belong there. The original HTML is a reference, never a runtime dependency.

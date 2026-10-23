# Caecilia Notch Island

Native C++20 Windows overlay: D3D11, DXGI, DirectComposition, Direct2D 1.1 and
DirectWrite. No web UI or outgoing network requests.

**M0 accepted by the user; M1 implemented and awaiting visual acceptance.**
M2–M4 have not started. See [M0](docs/m0-status.md), [M1](docs/m1-status.md),
and [performance](docs/perf.md). M0 v0.2 fixes pass MSVC Debug/Release builds and
CTest; manual acceptance procedures are in [M0](docs/m0-status.md#m0-v02-follow-up).

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

Provision Visual Studio Desktop development with C++, Windows SDK and CMake 3.25+ locally.
Verified here with VS 2026 / MSVC 19.51 and Windows SDK 10.0.26100.0.
No build step downloads packages.

```powershell
cmake -S . -B build-msvc -G "Visual Studio 18 2026" -A x64
cmake --build build-msvc --config Debug
ctest --test-dir build-msvc -C Debug --output-on-failure
cmake --build build-msvc --config Release
ctest --test-dir build-msvc -C Release --output-on-failure
.\build-msvc\Release\CaeciliaNotchIsland.exe --demo --debug-bounds --show-debug-fps
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
| `1` | Dormant, unpin, cancel HUD |
| `2` | Compact with active waveform, unpin, cancel HUD |
| `3` | Expanded (pinned), cancel HUD |
| `4` | Pin Volume HUD until another state selection |
| `V` / `B` / `P` | Volume / Brightness / Pomodoro HUD for 1800 ms; refresh on each press |
| `Space` | Fake play/pause (wave on/off; playing also selects Compact) |
| `C` | Close fake media source: Dormant + clear pin |
| Hover | Swell at once (+8/+2/+1); Expand after `hoverDelayMs` (default 150 ms); collapse 250 ms after leaving |
| Left click Dormant/Compact | Expand immediately and pin |
| Left click empty area (Expanded) | Toggle pin; unpinning collapses and suppresses hover until you leave |
| Mouse down outside (pinned) | Unpin and collapse; the click still reaches the app below |
| Middle click Compact | Fake play/pause, no expand (SMTC in M2) |
| Click tab | Change tab with sprung pill and content fade; pin unchanged |
| Ctrl+Alt+F12 | Toggle all implemented layout bounding boxes |
| Ctrl+Shift+F | Toggle active FPS / settled `IDLE` |

The HUD has priority over hover. A timed HUD returns to the current base/hover
state. `V/B/P` replace a pinned HUD with a timed one. Key auto-repeat does not
retrigger. `1`/`2` unpin with the cursor on the island, which sets `suppressHover`,
so the chosen base stays visible until you leave/re-enter. Ctrl/Alt/Shift combinations are not demo inputs.

Data is **fictional and static** in M1: media controls, seeking, real files,
telemetry polling and system volume/brightness actions arrive in later milestones.
The clock samples local time when rendering; it does not have a periodic M1 timer.

With Windows animation effects off, shapes/tab positions snap, waves become static
10-DIP bars, and both fades take 100 ms with no delay or blur.

## Options and shutdown

- `--demo`: enable the keys above; start Dormant.
- `--demo-media-active`: with `--demo`, show active Dormant dot.
- `--debug-bounds`, `--show-debug-fps`: start with the respective debug overlay.
- `--config <path>`: use an alternative JSON file instead of the default path below.
  The old `showDebugFps=true` text format is replaced by JSON; optional
  `"showDebugFps": true` still works. `--show-debug-fps` overrides that value.
- `--verify-device-recovery`: tear down GPU resources after the first Present and
  recreate them. This is **not** a real GPU driver-failure injection.

Only one instance runs. No tray/menu until M3. Close through Task Manager, or:

```powershell
Stop-Process -Name CaeciliaNotchIsland
```

Errors go to `OutputDebugString`. The startup OS check uses `RtlGetVersion`;
Windows builds below 22000 show **Notch Island membutuhkan Windows 11** and exit.

## User configuration

At startup, `%APPDATA%\NotchIsland\config.json` is resolved through
`SHGetKnownFolderPath(FOLDERID_RoamingAppData)`. A missing file/folder is created once:

```json
{
  "version": 1,
  "titleFont": "serif",
  "accentColor": "#38BDF8",
  "hoverDelayMs": 150,
  "monitorIndex": 0
}
```

- `titleFont`: `serif` (Georgia) or `sans` (Segoe UI Variable Display), same title metrics.
- `accentColor`: `#RRGGBB`; changes accents/gradients/glow, not labels, surfaces or Focus orange.
- `hoverDelayMs`: integer clamped to 0–1000; invalid types use 150. Collapse remains 250 ms.
- `monitorIndex`: 0 is primary; remaining monitors are sorted left-to-right (then top/device name).
  Unavailable indices fall back to primary; display changes re-evaluate the requested index.
- Missing/invalid fields use their own defaults; unknown fields remain untouched.
  Broken JSON/UTF-8 or files above 64 KiB use all defaults, log a warning, and are never overwritten.
- Restart after editing. Tray **Reload config** belongs to M3; no automatic hot reload.
- JSON parsing uses Windows' native `Windows.Data.Json`, without third-party dependencies.

## Architecture and verification

- Fixed 492 × 193 DIP HWND at configured-monitor `rcMonitor` top center.
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

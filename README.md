# Caecilia Notch Island

Native C++20 Windows overlay using D3D11, DXGI, DirectComposition, Direct2D 1.1,
and DirectWrite. No web UI or outgoing network requests.

**Current milestone: M0 implemented, awaiting MSVC and Windows visual acceptance.**
M1–M4 have not been started. See [docs/m0-status.md](docs/m0-status.md) and
[docs/perf.md](docs/perf.md) for validation status and unmeasured targets.

## Build on Windows 11 / Windows 10 build 19041+

Install/provision Visual Studio 2022 with Desktop development with C++, an
appropriate Windows SDK, and CMake 3.25+ locally. No build step downloads packages.

```powershell
cmake -S . -B build -G "Visual Studio 17 2022" -A x64
cmake --build build --config Debug
ctest --test-dir build -C Debug --output-on-failure
cmake --build build --config Release
```

Both app and C++ tests use `/W4 /WX /permissive- /utf-8 /EHsc`. CMake intentionally
rejects non-MSVC application builds. M0 needs only Windows SDK COM headers;
C++/WinRT media arrives in M2. An offline NuGet/vcpkg C++/WinRT include directory
may be supplied with `-DCAECILIA_CPPWINRT_INCLUDE_DIR=...` (contains `winrt/base.h`).

## Run M0

```powershell
.\build\Release\CaeciliaNotchIsland.exe --demo --debug-bounds --show-debug-fps
```

- `--demo`: deterministic static Dormant; no real media or telemetry needed.
- `--demo-media-active` with `--demo`: use the approved active Dormant dot opacity.
- `--debug-bounds`: start with body/ear/lens/status bounding rectangles visible.
- `--show-debug-fps`: start with `IDLE` visible in the specified debug rect.
- `--config <path>`: read an explicitly selected local config file.
- `--verify-device-recovery`: release all GPU/composition resources after the first
  successful frame and rebuild on the next event. This tests resource lifecycle,
  **not** a genuine injected GPU driver/device failure.

Configuration format (UTF-8 / ASCII):

```ini
# caecilia.conf
showDebugFps=false
```

Command-line flags override config. Unknown keys/arguments fail with a debugger
log and exit code 1. No implicit file creation or config persistence.

### Shortcuts

- **Ctrl+Alt+F12:** bounding rectangles (Aqua, 1 DIP).
- **Ctrl+Shift+F:** independent FPS overlay toggle.

If another application owns a shortcut, a debugger diagnostic is emitted and the
command-line/config alternatives remain available. M0 always renders a settled
state, so FPS reads **IDLE**; no refresh timer runs just to refresh this label.
Keys 1–4 and V/B/P are reserved for M1, not implemented in M0.

No tray menu exists until M3. Close M0 through Task Manager, or:

```powershell
Stop-Process -Name CaeciliaNotchIsland
```

For a graceful test shutdown, send `WM_CLOSE` from a local Win32 test tool/debugger.
Startup/runtime errors use `OutputDebugString` rather than a focus-stealing dialog.

## M0 architecture

- Fixed 492 × 193 DIP HWND, with primary-monitor `rcMonitor` anchoring.
- `SetDpi` handles rendering scale; only HWND placement/size and input coordinates
  cross the physical-pixel/DIP boundary.
- Required layered/no-redirection/no-activate/topmost/tool window styles.
- Raw mouse input (`RIDEV_INPUTSINK`) updates geometry-based click-through without
  a periodic cursor timer. `WS_EX_TRANSPARENT` changes only when inside/outside
  changes. Raw input never causes an idle `Present`.
- Shared cached path for hit testing, surface and content mask.
- Premultiplied composition flip swap chain, transparent clear, clipped Dormant
  contents; no Dormant surface rim or drop shadow (status glow is separate).
- Blocking message loop. Paint occurs only on invalidation. Device-loss retries
  are bounded, with an operational backoff rather than a busy render loop.
- DirectWrite system-font fallback, exact line height derived from font metrics,
  character spacing, tabular figures and grayscale antialiasing.

**Runtime caveat:** layered + no-redirection + DirectComposition visibility and
cross-process click-through must be checked on the actual Windows/DWM/driver
combination. Raw-input processing is asynchronous; test fast boundary crossings
and clicks rather than assuming an absolute no-race guarantee.

## Specifications and offline checks

`PRD.md` defines scope; `DESIGN.md` and its synchronized
`src/spec/NotchIslandSpec.h` define visual values. The HTML file is the original
reference prototype, not a runtime dependency. Do not open its CDN-dependent UI
when performing a strictly offline validation; the following verifier executes
only its data/export code, without loading the browser UI:

```powershell
node tools/verify-spec.cjs
```

The verifier checks Appendix A equality, original HTML declaration values and
geometry builder, allowing only explicitly approved M0 additions. Node is a
local development tool, never an application dependency.

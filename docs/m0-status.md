# M0 — implementation and handoff

## Implemented

- C++20 MSVC-only CMake project with `/W4 /WX`, no dependency downloads.
- Required non-activating/topmost/tool/layered/no-redirection popup, single instance.
- Fixed 492 × 193 DIP client envelope; primary `rcMonitor` top/center positioning.
- Per-monitor DPI v2, WM_DPICHANGED / WM_DISPLAYCHANGE repositioning, SetDpi drawing.
- D3D11 + premultiplied DXGI composition flip swap chain + DirectComposition + D2D.
- Shared cached spec geometry for surface, content clipping, and FillContainsPoint.
- WM_NCHITTEST HTCLIENT only inside shape; HTTRANSPARENT elsewhere.
- Raw mouse input plus inside/outside WS_EX_TRANSPARENT toggling across applications.
- Dormant Core surface, lens/ring, status dot with approved idle/active opacity,
  clipped glow; no surface rim/drop shadow.
- Independent Ctrl+Alt+F12 bounds and Ctrl+Shift+F FPS toggles, command-line fallbacks.
- Debug FPS uses the specified micro-label IDLE layout and has no periodic repaint.
- Static deterministic `--demo`; optional `--demo-media-active` for dot verification.
- Device recreation for D2DERR_RECREATE_TARGET / DXGI device removed/reset/hung;
  bounded retries avoid a GPU-error busy loop; resume tears down/rebuilds resources.
- DirectWrite seven-role formats, checked font fallback, metrics-derived baseline,
  tracking and tabular figures for debug layout.
- Updated DESIGN Appendix A/spec and PRD debug wording from final user decisions.

## Checks actually executed

- PASS: `node tools/verify-spec.cjs` — Appendix A equality and original HTML values.
- PASS: all C++ translation units with supplemental MinGW `-fsyntax-only`, C++20,
  `-Wall -Wextra -Wconversion -Werror`.
- PASS: supplemental MinGW geometry executable — body hit, outside non-hit,
  envelope bounds, all state path construction and zero-ear case.
- MSVC build, `/W4` warning verification, app runtime rendering and screenshots:
  **not executed**. Standard `vswhere.exe` / VS 2022 `vcvars64.bat` locations checked;
  toolchain not found. Per user instruction, implementation continued for local handoff.

The supplemental compiler is not the supported distribution toolchain.

## Windows 11 acceptance checklist

Run build commands in README, then verify:

- [ ] Debug and Release MSVC builds and CTest pass without warnings.
- [ ] Static Dormant appears at physical monitor top center at 100% and 150%.
- [ ] Bounds match body, ears, lens, status and optional FPS rect from DESIGN.
- [ ] Dot idle .35 and demo-active .85 match approved spec; IDLE alpha is .30 once.
- [ ] Composition background remains transparent with required combined styles.
- [ ] Clicks outside shape reach Notepad/Explorer (different process), including
      rapid boundary crossings; inside clicks do not activate overlay.
- [ ] Debug toggle hotkeys work or command-line fallback works on hotkey collision.
- [ ] Raw input outside the shape does not produce Presents; stationary idle sleeps.
- [ ] Second instance exits without stealing focus or creating a second overlay.
- [ ] DPI, resolution, primary display and taskbar-at-top changes anchor correctly.
- [ ] `--verify-device-recovery` produces a second valid frame after resource reset.
- [ ] Actual D2D/DXGI device loss, resume, and display changes recover without crash.

## Known boundaries

- M1 motion/hover/state transitions and demo 1–4/V/B/P are intentionally absent.
- Media/telemetry/system watchers/tray/menu/autostart/drop tray belong to M2–M4.
- No recurring FPS readout is needed in static M0: the final frame says IDLE.
- Config is read only from explicit `--config`; no network or implicit persistence.
- `--verify-device-recovery` tests teardown/rebuild, not hardware device-loss injection.
- Raw input is asynchronous: the specified style toggle cannot mathematically
  guarantee zero click race during every rapid mouse boundary crossing.
- Combined WS_EX_LAYERED / WS_EX_NOREDIRECTIONBITMAP / DirectComposition behavior
  requires real DWM/driver validation. No silent fallback to another UI stack.
- On persistent device loss, retries pause after the bounded budget; a new paint,
  display/resume event, or debug toggle can initiate recovery again.
- No measurements are invented; see `docs/perf.md`.

## User acceptance — 2026-10-19

The user confirmed all three requested desktop checks passed: visual placement/
transparency, debug toggles, and cross-process click-through/no focus stealing.
M0 is accepted for proceeding to M1, explicitly authorized by the user.
This does not assert separate 100%/150% measurements, MSVC verification, driver
failure injection, or performance captures; the detailed unmeasured items above
remain unchecked. Implementation notes in this document describe the M0 snapshot.

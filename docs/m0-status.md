# M0 — preparation status

## Completed

- Read PRD.md and DESIGN.md in full, including Appendix A.
- Inspected the HTML prototype's data, layout, and C++ export generator.
- Copied DESIGN Appendix A verbatim into `src/spec/NotchIslandSpec.h`.
- Added `tools/verify-spec.cjs`: offline comparison with DESIGN and HTML export.
- Kept original requirements and HTML unchanged.

The HTML export agrees numerically with the header for state geometry, element
rectangles, colors, and springs; its geometry builder also agrees. Some exported
symbol names and comments differ, so the required DESIGN copy is preserved.

## Decisions still required before overlay implementation

1. **Incomplete exported constants.** Neither Appendix A nor the HTML C++ exporter
   supplies named constants for the fixed window envelope, lens/debug stroke,
   status opacity, glow blur, or typography. Some are specified elsewhere in
   DESIGN but the hard rule allows visual numbers only in the spec header.
   Permission is needed to extend DESIGN Appendix A and the header together.
2. **Anchor conflict.** DESIGN section 2 specifies monitor top, not work area;
   the header and HTML export comment say work-area top. Confirm monitor top and
   authorize correcting the comment in the relevant sources.
3. **Physical px versus DIP.** Lens and debug strokes are described as 1 px,
   while the global unit is DIP. Confirm whether these remain one physical pixel
   at non-100% scaling or are one DIP.
4. **Debug FPS layout.** Neither document nor HTML specifies the FPS rect,
   typography role, color, and idle reporting behavior. These cannot be guessed.
5. **Dormant status discrepancy.** DESIGN section 5 / HTML layout metadata say
   opacity 0.85; HTML `IslandContent` uses 0.35 when media is not playing. Confirm
   the intended M0 Dormant appearance and whether the prototype's extra lens
   highlight (absent from DESIGN) should be excluded.
6. **Cross-process click-through.** `HTTRANSPARENT` alone forwards hit testing
   only within the same thread. Approve an additional mechanism to meet the
   cross-application requirement; it must preserve the transparent composition
   envelope for future shadow rendering.

## Local toolchain observations

- CMake 4.2.3 and Node.js 24.15.0 are available.
- Initial PATH/default-install/registry checks did not locate MSVC, Visual Studio
  Installer, an official Windows SDK, or C++/WinRT.
- MinGW is available but is not an authorized substitute for the required MSVC
  build. A custom toolchain location or offline installation is needed.
- No dependencies have been downloaded and no network requests are made by the
  verification script.

## Planned M0 files after clarification

- `CMakeLists.txt`: C++20, MSVC `/W4`, offline dependencies.
- `src/main.cpp`: DPI v2, single-instance mutex, error boundary, message loop.
- `src/app/App.{h,cpp}`: module wiring and event-driven redraw requests.
- `src/window/OverlayWindow.{h,cpp}`: required styles, no activation, monitor
  positioning, DPI/display changes, shape hit testing.
- `src/render/Renderer.{h,cpp}`: D3D11 / premultiplied flip-model composition swap
  chain / DirectComposition / D2D 1.1, resource recovery on device loss.
- `src/render/IslandGeometry.{h,cpp}`: call the spec builder and cache the path.
- `src/render/Painter.{h,cpp}`: static Dormant and debug rectangles.
- `src/render/TextStyles.{h,cpp}`: confirmed debug text style and font fallback.
- `src/data/DemoSource.{h,cpp}`: deterministic M0-only demo input.

## M0 acceptance checks — not yet run

- [ ] MSVC build at `/W4`, no new warnings.
- [ ] `--demo` displays static Dormant at monitor top center.
- [ ] Correct body, ears, lens, and status rectangles with Ctrl+Alt+F12.
- [ ] Debug FPS obeys confirmed spec and does not force idle presents.
- [ ] Visual comparison at 100% and 150%, with at most 1 px difference.
- [ ] Clicks outside shape reach a different application; shape remains interactive.
- [ ] Never activates or appears in taskbar; second instance exits safely.
- [ ] DPI/display changes reposition correctly without per-frame HWND resizing.
- [ ] No repeated Present while unchanged; blocking idle message loop.
- [ ] Device-loss recreation, sleep/resume, resolution-change checks.

M0 is **not complete**. No performance numbers are available yet.

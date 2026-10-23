# M1 — shape, motion and demo

## Implemented

- All seven geometry/appearance springs use fixed 1/240 s semi-implicit Euler,
  1/20 s max elapsed per frame and preserved velocities on interruption.
- Shared current path, fixed HWND, Core/Obsidian interpolation, internally masked
  rim with origin pixel snapping, shadow SPEED during animation/cached at rest.
- State precedence HUD > Expanded > Compact > Dormant; hover entry/exit 150/250 ms (v0.2).
- Hover Swell (+8 W / +2 H / +1 r, `Spring::HoverSwell`) from the first frame; ToExpanded
  takes over with preserved velocity; no swell in HUD/Expanded/pinned/suppressHover/reduced motion.
- Click-pin, empty-area pin toggle, `suppressHover`, outside unpin via `WH_MOUSE_LL` installed
  only while pinned (post-only, always `CallNextHookEx`). Pin survives HUD and base changes.
- Content origin `x = (W_current − W_target) / 2` keeps layouts centered during morph/swell.
- Pointer re-sampled after the raw-input burst (WM_INPUT can precede the cursor update).
- Demo keys 1–4/V/B/P/Space/C only while pointer is inside; no focus capture/key suppression.
  Middle click on Compact toggles the fake wave (SMTC in M2).
- Pinned key-4 HUD, transient 1800 ms HUD refresh, return to base/hover state.
- Incoming delayed cubic fade/blur and outgoing linear fade; tab pill spring.
- All static demo layouts: Compact, three Expanded tabs, Volume/Brightness/Pomodoro.
- Reduced motion: snap shapes/tab, static 10-DIP waveform bars, 100 ms fades without delay/blur.
- Dynamic bounds and independent FPS/IDLE; no periodic idle frame/FPS refresh.
- Frame latency wait handle drives animation pacing; message/deadline wait otherwise.
- Dynamic FillContainsPoint and transparency follow the actual sprung geometry.

## Checks performed

- PASS: offline Appendix A/header/original HTML value checks.
- PASS: full alternative MinGW C++20 portable build using
  `-Wall -Wextra -Wconversion -Werror`, static compiler runtime.
- PASS: geometry tests including all state paths, envelope, hit tests and zero ear.
- PASS: spring transitions, retarget velocity, 60/120/144/240 Hz fixed-step equivalence,
  frame clamp, settling/snap, HUD precedence/refresh/pin/dismiss and hover cancellation.
- PASS: startup smoke test remained alive/responding after five seconds.
- PASS (v0.2, injected mouse/keys at 100%, screenshots): swell before delay, hover expand,
  still open 150 ms after leave, collapsed after 250 ms, click-pin survives leave, outside
  click unpins, empty-area click unpins + suppresses re-open, re-entry swells again,
  `2` Compact, middle click toggles wave without expanding, Compact swell, `C` → Dormant.
- Code review fixed tab transition consuming pre-click idle time and content glyphs
  accidentally inheriting a previous frame's body/window transform.
- PASS: MSVC 19.51 (VS 2026) Debug + Release `/W4 /WX`; CTest geometry, motion, config,
  render, spec-consistency in both configs (`cl` need not be on PATH; CMake finds VS).
- PASS (`tools/verify-m1-desktop.ps1`, 100%, normal and animation-effects-off runs,
  36 captures in `artifacts/m1-validation/desktop`): 492×193 envelope, overlay never
  foreground, all states/tabs/HUD kinds, pin/unpin, `C` while hover-expanded → Dormant,
  clean exit code 0.
- PASS (`render` CTest, offscreen WARP at 96 and 144 DPI): bitmap 492×193 / 738×290,
  transparent envelope, reduced Compact frames identical over time, normal ones differ.
- PASS (`tools/compare-m1-reference.py` vs `tools/capture-m1-reference.cjs` HTML prototype):
  island silhouette within ≤ 1 px at 96 and 144 DPI for all 8 layouts. Text glyphs are not
  pixel-compared (DirectWrite vs Chrome rasterize differently).
- Not verified: real Windows 150% scaling (`GetDpiForWindow`/`WM_DPICHANGED`), multi-monitor,
  sleep/resume and device loss on hardware, target-machine performance.

## M1.3 validation — 2026-10-23

Baseline: `7165014`, branch `feat/m1-foundation-validation`. This pass changes tests
and documentation only; no production failure was reproduced, and visual tokens /
spring parameters are unchanged.

| Check | Result in this pass | Coverage / limitation |
|---|---|---|
| MSVC Debug + Release | PASS | `/W4 /WX`; CTest 5/5 in each configuration |
| Spec consistency | PASS | `node tools/verify-spec.cjs` |
| Four states, tabs, three HUD kinds | PASS, offscreen | WARP/D2D captures at 96/144/192 DPI; 42 PNGs under `artifacts/m1-validation/render` |
| Rapid interruptions | PASS, unit test | 240 state retargets preserve all seven positions/velocities, stay finite/in-envelope, then settle; 240 tab retargets settle |
| Boundary hit-tests | PASS, unit test | All four silhouettes at 100/150/200%; half-physical-pixel top/bottom samples, ears/cutouts, adjacent tabs and morphing content origin |
| Click cancellation | PASS, unit test | Same target fires once; different target, outside→inside, inside→outside, cancel and button mismatch do not fire |
| Clock and reduced motion | PASS, unit/offscreen tests | Minute rollover math; reduced Compact captures identical over time, normal Compact differs |
| Desktop interaction / smoothness | NOT RUN in this pass | Prior desktop results above are historical, not a rerun of this baseline |
| Real DPI/monitor change | PENDING | Offscreen `SetDpi` does not validate `GetDpiForWindow`, `WM_DPICHANGED`, monitor selection or composition |
| Sleep/resume | PENDING | Frame-delta clamp is tested; an actual sleep/resume cycle is not |
| `DXGI_ERROR_DEVICE_REMOVED` | PENDING | Existing `--verify-device-recovery` only tears down resources; it does not exercise the device-removed error path |

Desktop control is unavailable in this session. No display/animation settings,
power state or GPU driver were changed. Existing `tools/verify-m1-desktop.ps1`
was not run; its captures require visual inspection (the script asserts envelope,
focus and clean shutdown, not every depicted state).

### Remaining acceptance run

Use the current MSVC Release build (the portable artifact was not rebuilt):

```powershell
.\build-msvc\Release\CaeciliaNotchIsland.exe --demo --debug-bounds --show-debug-fps
```

- At real 100/150/200% scaling, exercise 1→3→4→2→1 rapidly, all tabs, and clicks
  just inside/outside the moving outline. Check momentum, corners, click-through
  to a harmless target window, no stolen focus, and no action on mismatched releases.
- Change scaling while running; change/disconnect the configured monitor. Confirm
  top-center placement, correct envelope, aligned hit-tests and cancelled presses.
- Leave Expanded across a minute boundary; verify the clock changes. Check IDLE
  after settling in Dormant/Expanded/HUD and continued active Compact waves.
- Sleep/resume in Expanded and Compact; verify redraw, input and transitions recover.
- Run `--verify-device-recovery` separately for resource recreation only. Actual
  device-removed handling still needs a controlled fault-injection/debugger run;
  record the HRESULT, successful subsequent Present and bounded retries before PASS.
- Record observed results here and obtain explicit user acceptance. **M1.3 remains
  pending hardware/visual acceptance; M2 is blocked.**

## Bugs found and fixed during validation

- `C` (close media) while Expanded via hover stayed Expanded; now clears the hover latch and
  suppresses re-open until the pointer leaves (test in `MotionTests.cpp`).
- Filled rect edges were not pixel-snapped at 150% (divider right edge at 484.5 px); fills,
  accents, divider and the frame origin now snap to device pixels; rim and mask share one origin.
- `IslandState` numbered 0–3; DESIGN §3 requires 1–4.

## Local visual checklist

Run `artifacts/M1-portable/Jalankan Demo.bat`; old M0 must not still be running.
Place the pointer in the island for demo key controls. `[x]` = verified by the scripted
desktop run at 100%; `[ ]` = still needs a human or hardware.

- [x] Keys 1/2/3/4 reach all four states; no black rectangular envelope.
- [x] 4 remains HUD; V/B/P refresh 1800 ms and restore the appropriate state.
- [ ] Fast interrupted transitions carry momentum smoothly, without sharp corners (needs eyes).
- [x] Hover swells at once, expands after 150 ms; leaving collapses after 250 ms.
- [x] Click pins; outside click unpins; empty-area click unpins.
- [x] Three tabs show correctly placed content/glyphs (pill animation smoothness: needs eyes).
- [ ] Ctrl+Alt+F12 rects match DESIGN at 150% on a real 150% display.
- [ ] Ctrl+Shift+F shows FPS while moving/waving, then IDLE for settled non-wave states.
- [ ] Compact active waveform continues; Dormant/Expanded/HUD settle and stop Present.
- [ ] Transparent area clicks reach other apps during animation (needs a target app).
- [x] Windows animation effects off: snaps, static bars, no blur.
- [ ] Resource recreation, sleep/resume and DPI/display changes recover correctly.

## Boundaries

M1 media/telemetry/levels/files are deterministic **static samples**, not system
values. Now Playing controls/seek/file operations are presentation-only until later
milestones. Clock repaints at each local minute boundary only while Expanded is shown. No SMTC or other M2 service is started.

Rapid repeated content changes keep the newest incoming/outgoing pair rather than
retaining unbounded historical layers.
The raw-key observer never records text and does not consume foreground keystrokes.

M0 acceptance was reported by the user; M1 awaits acceptance. Do not start M2.

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
- Reduced motion: snap shapes/tab, static minimum waveform, 100 ms fades without delay/blur.
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
- MSVC `/W4 /WX`, visual animation/alpha-effects correctness and target-machine
  performance: **not yet verified**. No numerical performance claims.

## Local visual checklist

Run `artifacts/M1-portable/Jalankan Demo.bat`; old M0 must not still be running.
Place the pointer in the island for demo key controls.

- [ ] Keys 1/2/3/4 reach all four states; no black rectangular envelope.
- [ ] 4 remains HUD; V/B/P refresh 1800 ms and restore the appropriate state.
- [ ] Fast interrupted transitions carry momentum smoothly, without sharp corners.
- [ ] Hover swells at once, expands after 150 ms; leaving collapses after 250 ms without flicker.
- [ ] Click pins; outside click unpins and still reaches the app; empty-area click unpins.
- [ ] Three tabs animate the pill and show correctly placed content/glyphs.
- [ ] Ctrl+Alt+F12 rects match DESIGN at 100% and 150%; HWND envelope does not animate.
- [ ] Ctrl+Shift+F shows FPS while moving/waving, then IDLE for settled non-wave states.
- [ ] Compact active waveform continues; Dormant/Expanded/HUD settle and stop Present.
- [ ] Transparent area clicks reach other apps during animation; inside clicks never activate.
- [ ] Windows animation effects off: snaps, no waves/blur, 100 ms content fades.
- [ ] Resource recreation, sleep/resume and DPI/display changes recover correctly.

## Boundaries

M1 media/telemetry/levels/files are deterministic **static samples**, not system
values. Now Playing controls/seek/file operations are presentation-only until later
milestones. Clock refreshes on render only. No SMTC or other M2 service is started.

Rapid repeated content changes keep the newest incoming/outgoing pair rather than
retaining unbounded historical layers. Exact visual comparison remains pending.
The raw-key observer never records text and does not consume foreground keystrokes.

M0 acceptance was reported by the user; M1 awaits acceptance. Do not start M2.

# Roadmap M1 → M2 → M3.1

Session handoff document. Read this first in a new session, then `DESIGN.md`,
`PRD.md` (milestones, lines 178–184) and `docs/m1-status.md`.

## Working rules

- Never work on `main`. Branches: M1 → `feat/m1-foundation-validation`,
  M2 → `feat/m2-gsmtc-media-hub`, M3.1 → `feat/m3.1-core-audio-hud`.
- Preserve local changes. No `git reset --hard`, `git checkout -- .` or other destructive commands.
- No automatic `git commit` / `git push`. At the end of every sub-stage report:
  focused diff summary, `/W4` build results, `ctest` results, `node tools/verify-spec.cjs`,
  and a ready `git add` / `git commit -m` / `git push -u origin <branch>` block.
- Phase gate: one sub-stage at a time, then stop and wait for user confirmation.
- Do not change spec shape, colors, sizes or spring character unless validation proves a regression.

## Build and test

```powershell
cmake -S . -B build-msvc -G "Visual Studio 18 2026" -A x64
cmake --build build-msvc --config Debug;   ctest --test-dir build-msvc -C Debug --output-on-failure
cmake --build build-msvc --config Release; ctest --test-dir build-msvc -C Release --output-on-failure
node tools/verify-spec.cjs
```

## Stages

| Stage | Scope | Done when | Status |
|---|---|---|---|
| M1.1 Pointer accuracy | Shared pixel snapping for paint and hit-test; click fires only on release over the pressed target; cancel on `WM_CAPTURECHANGED` / `WM_CANCELMODE` / reposition / close | DPI 100/150/200% tests; outside-down/inside-up and inside-down/outside-up do not fire; no hover/pin/click-through regression | Committed `7165014` |
| M1.2 Light updates | Clock updates on minute boundaries only while visible in Expanded; reduced-motion refresh separate from GPU reset; settled states do not poll | Expanded clock does not freeze; idle states wait without periodic wakeups; reduced motion still correct | Committed `7165014` |
| M1.3 Validation | Four states, fast transitions, tabs, boundary hit-tests, DPI/monitor change, sleep/resume, device lost (`DXGI_ERROR_DEVICE_REMOVED`); fix only what fails | Results recorded in `docs/m1-status.md`; user accepts M1 | Automated checks pass; hardware/visual acceptance pending |
| M2.1 GSMTC data | C++/WinRT GSMTC, deterministic session pick, metadata/timeline/artwork via async events off the UI thread, bounded artwork, drop stale results by session identity, clean unsubscribe on shutdown, keep `--demo` | Spotify/browser session appears; track/source change leaves no stale data | Blocked on M1 |
| M2.2 Compact & pause | `DESIGN.md:97–109`: Playing → Compact; pause → 30 s grace (3×3 DIP dot, art 0.5, 200 ms); resume cancels; grace end → Dormant honoring pin/hover/HUD; session closed clears media and pin | Deterministic time tests pass | Blocked |
| M2.3 Controls & scrub | Middle-click play/pause in Compact without expand; Prev/Play/Next per session capability; progress only while visible; scrub with capture/cancel, range check, seek coalescing, commands bound to session ID; no silent global media-key fallback | Seeks never complete out of order; API failure is not reported as success | Blocked |
| M2.4 Stability | Spotify + browser alone/together; no art/duration/seek; fast switches during async; app close; sleep/resume; real metrics in `docs/perf.md` | User accepts M2 | Blocked |
| M3.1 Volume HUD | Core Audio volume/mute callbacks, endpoint rebind on device change, startup baseline shows no HUD, `StateMachine::TriggerHud` | Volume/mute/device change correct; no HUD at startup | Blocked on M2 |

Out of scope: weather, online lyrics, lookup, split island, audio loopback capture.

## Reference project

`devcode90/Dynamic-Island-for-Windows`, snapshot `7fe506e81e4e72460251aa60110437794dbc88da`,
MIT © 2026 devcode90 (keep the notice if substantial code is copied).

- Keep Caecilia's D3D11/DXGI/DirectComposition/D2D renderer, deadline scheduler and state machine.
- Reuse patterns only: Core Audio callbacks + endpoint rebind + silent baseline (`BindVolumeEndpoint`);
  minimal GSMTC with session identity (`MediaThreadProc`); Shell `IDropTarget` for the drop tray.
- Avoid: Windhawk host, polling, detached workers, unsynchronized globals, timeout-only shutdown, network features.

## M1.1 implementation notes

- `src/render/IslandGeometry.h`: `SnapToPixel`, `BodyOrigin`, `BodyPointFromClient` (single source for paint and hit-test).
- `src/input/ClickGesture.h`: `ClickTarget`, `TargetAt`, `ClickGesture` (Down/Up/Cancel).
- `WindowEvents::ClickTargetAt` + `OnClick(ClickTarget)` replace the point-based `OnClick` / `OnMiddleClick`.
- `--demo` middle-click toggles only the fake waveform; real SMTC arrives in M2.3.

## M1.2 implementation notes

- `src/app/Clock.h`: `MsToNextMinute(SYSTEMTIME)` (tested in `MotionTests.cpp`, minute/hour/day/year/leap second).
- `App::clockDeadline_` is armed in `App::Tick` only while the incoming layer is Expanded and joins
  `App::DeadlineWait`; other states clear it, so they wait on input only. `WM_TIMECHANGE` re-arms via `OnDraw`.
- `WM_SETTINGCHANGE` → `App::OnSettingChange` (re-reads `SPI_GETCLIENTAREAANIMATION`, repaints only on change).
  `App::OnResume` (power resume) alone calls `renderer_.Reset()`.
- Smoke: `--demo` Dormant, CPU time unchanged over 3 s idle.

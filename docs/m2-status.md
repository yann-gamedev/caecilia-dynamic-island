# M2.1 — GSMTC data

Started on `feat/m2-gsmtc-media-hub` at the user's explicit request. This permits
M2.1 work; it does not mark pending M1 hardware/visual checks as passed.
The previously requested HTML removal remains in the working tree.

## Scope

- Native C++/WinRT GSMTC data source, event-driven rather than periodically polled.
- Windows current-session preference with deterministic fallback; object identity
  and refresh revisions prevent older async results replacing newer data.
- Metadata, playback state and event-sampled timeline delivered as immutable
  snapshots; artwork is bounded and decoded off the UI thread.
- UI waits for media updates alongside messages/frame latency. GPU artwork is
  cached by CPU artwork identity and rebuilt after device recreation.
- Without `--demo`, hover/click Expanded → Now Playing displays real media data.
  No session uses an empty view; absent/invalid artwork uses the existing placeholder.
- `--demo` retains deterministic M1 samples and does not initialize GSMTC.

## Deliberate limits

- M2.2 owns automatic Compact selection and pause grace/visuals. M2.1 stays Dormant
  until hovered/clicked, while the status dot reflects the selected playback state.
- M2.3 owns playback commands, scrubbing and timeline extrapolation. M2.1 progress
  only updates on provider events, so a provider's infrequent events can look static.
- Telemetry, Drop Tray and HUD values remain demo samples, not live system data.
- No network artwork fetch, media-key fallback, detached worker or runtime dependency download.
- Current source requires MSVC + Windows SDK C++/WinRT. Existing MinGW portable
  artifacts are historical M1 demos, not M2 builds.

## Acceptance (manual, pending)

Run `build-msvc\Release\CaeciliaNotchIsland.exe` without `--demo`, then open
Expanded / Now Playing. Close any existing island first (single-instance app).

1. Spotify alone; browser media alone: title, artist, artwork and timeline appear.
2. Both: Windows current session is preferred; fallback selection remains stable.
3. Rapid track/source changes, including two sessions from the same app: no old
   title/artwork reappears after the new snapshot. Empty artwork clears the old image.
4. Missing title/artist/duration, live stream and malformed/oversized artwork:
   no crash, no invalid progress or excessive allocation; placeholder where needed.
5. Close the selected source: remaining session or empty view replaces it.
6. Exit during metadata/artwork requests; repeat startup/shutdown: no hang,
   callback after destruction or retained subscription. Check sleep/resume separately.
7. Run `--demo`: original keys, states, tabs and static data still work without GSMTC.

Do not mark these scenarios passed from unit/offscreen tests alone. Stop after
M2.1 for user review; do not implement M2.2/M2.3 in this stage.

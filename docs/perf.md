# Performance / acceptance measurements

## Environment and evidence

M0 implementation environment: Windows shell; CMake 4.2.3; MSVC/official Windows
SDK not found at the checked standard locations. MinGW is present and used for
supplemental syntax checks and a geometry-only test executable. This is **not**
a substitute for the required MSVC build or visual acceptance on Windows 11.

No application performance figures have been measured. No NFR is claimed passed.

| Requirement | Target | M0 result |
|---|---|---|
| NFR-01 idle CPU | < 0.5% | Unmeasured; no continuous render/cursor timer in source |
| NFR-02 animation CPU | < 5% | Not applicable to static M0; measure M1+ |
| NFR-03 working set | < 40 MB | Unmeasured |
| NFR-04 startup | < 300 ms | Unmeasured |
| NFR-05 frame rate | monitor refresh; no idle rendering | Idle behavior requires Present capture; animation M1 |
| NFR-06 hover latency | ≤ 120 ms + one frame | M1 |
| NFR-07 distribution size | < 5 MB | Unmeasured; installer M4 |
| NFR-08 visual accuracy | ≤ 1 px at 100% / 150% | Pending screenshots on Windows |
| NFR-09 stability | device loss / resume / resolution | Recovery code present; runtime tests pending |
| NFR-10 privacy | no outgoing connections | No network API/dependency fetch path in implementation; runtime capture pending |

## Reproducible measurement plan

Record CPU model/core count, GPU/driver, Windows build, display resolution, refresh
rate, DPI, build configuration and commit for each run. Use Release for metrics.
Do not use the prototype browser UI as a performance baseline process.

1. Record cold/warm launch time to first successful Present using ETW/WPA.
2. Record process CPU and working set while Dormant with stationary pointer,
   while moving pointer outside the envelope, and while crossing the shape.
3. Use PresentMon/ETW with local tooling to confirm one final IDLE frame and no
   further Presents while unchanged. Debug text must not generate a frame timer.
4. Capture 100% and 150% with debug boxes off/on, compare body, ears, lens,
   dot and FpsRect against DESIGN. Use geometry, not the HTML's unlisted lens
   highlight or original browser font rendering, as the M0 layout authority.
5. Measure device teardown/recreation with `--verify-device-recovery`; separately
   test actual DXGI/D2D device-loss paths, sleep/resume and display/DPI changes.
6. Record executable/package size and include any redistributed runtime files.
7. Perform the eight-hour stability/memory-leak test during final M4 acceptance.

Keep raw captures under ignored `artifacts/`; publish measured summaries here.

# Spec patch v0.2 — jawaban untuk agent

Repo masih memakai spesifikasi v0.1. Semua nilai di bawah sudah final dan sama persis dengan `DESIGN.md` v0.2, `PRD.md` v0.2, dan `NotchIslandSpec.h` v0.2.

## Langkah untuk agent

1. Ganti tiga file ini dengan versi v0.2 (terlampir): `DESIGN.md`, `PRD.md`, `src/spec/NotchIslandSpec.h`. Jangan merge sebagian; ganti utuh.
2. Header v0.2 hanya **menambah** konstanta. Semua rect, warna, dan pegas lama tidak berubah, jadi kode M0/M1 yang sudah ada tetap valid.
3. Lanjutkan M1 dengan Bagian A–D di bawah. Nilai M2 (Bagian E) dicatat sekarang, tetapi baru dipakai di M2.

---

## A. Keputusan D1–D4 (final)

| # | Keputusan |
|---|---|
| D1 | Hover **dan** klik. Hover → Hover Swell seketika → Expanded setelah `hoverDelayMs` (default 150 ms) → collapse 250 ms setelah kursor keluar. Klik kiri di Dormant/Compact → Expanded seketika + **pinned**. Klik tengah di Compact → play/pause global, tanpa expand. |
| D2 | Media di-pause → Compact bertahan 30 000 ms: wave bar jadi titik 3 × 3 DIP, mini art opacity 0.5. Lewat grace → Dormant. Aplikasi media ditutup → Dormant seketika + clear pin. |
| D3 | Config di `%APPDATA%\NotchIsland\config.json` lewat `SHGetKnownFolderPath(FOLDERID_RoamingAppData)`. Field: `version`, `titleFont` (`serif`/`sans`), `accentColor` (`#RRGGBB`), `hoverDelayMs` (0–1000), `monitorIndex`. Default per field. |
| D4 | Windows 11 x64 saja (build ≥ 22000). Tanpa fallback Windows 10. |

Penomoran state tetap 1–4 (Dormant = 1).

## B. Mesin state

```
base      = Compact jika media Playing atau dalam grace pause, selain itu Dormant
expanded  = base ∈ {Dormant, Compact} && (pinned || (hovered && hoverDelay lewat && !suppressHover))
swell     = base ∈ {Dormant, Compact} && hovered && !expanded && !pinned && !suppressHover && !hud
tampil    = hud ? HUD : expanded ? Expanded : base
```

Prioritas: HUD > Expanded (pinned atau hover) > Compact > Dormant. Pin **bertahan** melewati HUD.

## C. Hover Swell

Bukan state, tetapi pengubah target geometri Dormant/Compact.

| Nilai | Delta | Konstanta | Dormant | Compact |
|---|--:|---|---|---|
| Width | +8 | `kSwellDeltaWidth` | 168 | 248 |
| Height | +2 | `kSwellDeltaHeight` | 30 | 36 |
| Bottom radius | +1 | `kSwellDeltaBottomRadius` | 13 | 18 |
| Ear radius | 0 | — | 8 | 9 |

| Pegas | Stiffness | Damping | Mass | Response | ζ | Overshoot | Settle |
|---|--:|--:|--:|--:|--:|--:|--:|
| `Spring::HoverSwell` | 503.6 | 27.83 | 1.0 | 0.28 s | 0.62 | 8.1% | 392 ms |

- Mulai seketika saat kursor masuk (frame berikutnya).
- Saat `hoverDelayMs` lewat, `Spring::ToExpanded` mengambil alih dari posisi **dan velocity** swell saat itu.
- Kursor keluar sebelum delay → kembali ke target tanpa swell dengan `Spring::HoverSwell`.
- Konten tidak berubah. Origin layer konten `x = (W_saat_ini − W_target) / 2`, jadi konten tetap di tengah (saat swell penuh: +4 DIP).
- Tidak ada swell di HUD, di Expanded, saat pinned, saat `suppressHover`, atau saat reduced motion.

## D. Klik dan pin

| Input / event | Saat | Hasil |
|---|---|---|
| Kursor masuk | Dormant, Compact | Swell seketika. Setelah `hoverDelayMs` → Expanded (tab terakhir). |
| Kursor keluar | Expanded tidak pinned | Collapse setelah `Timing::CollapseDelayMs` (250). Masuk lagi dalam 250 ms membatalkan. |
| Kursor keluar | Expanded pinned | Tidak terjadi apa-apa. |
| Klik kiri | Dormant, Compact | Lewati delay → Expanded **dan pinned**. |
| Klik kiri di area kosong | Expanded (hover) | Pin. |
| Klik kiri di area kosong | Expanded (pinned) | Unpin, collapse seketika, set `suppressHover`. |
| Klik kiri di kontrol | Expanded | Kontrol bekerja. Status pin tidak berubah. |
| Mouse down di luar island | Pinned | Unpin dan collapse. Klik tetap diteruskan ke aplikasi di bawah. |
| Klik tengah | Compact | Play/pause global. Tidak expand, tidak mengubah swell atau pin. |
| Klik tengah | State lain | Diabaikan. |
| Klik kanan | Semua | Diabaikan (cadangan menu konteks). |
| Fullscreen di foreground | Semua | Sembunyikan island (kecuali HUD), clear pin. |

- **Area kosong** = titik di dalam geometri island yang tidak ada di rect kontrol mana pun (tab, tombol, `ScrubHit`, tile).
- **`suppressHover`**: di-set saat unpin lewat klik ketika kursor masih di atas island, di-clear saat kursor keluar. Tanpa ini island langsung terbuka lagi 150 ms setelah ditutup.
- **Klik di luar**: pasang `WH_MOUSE_LL` hanya selama pinned dan lepas saat unpin. Callback hanya mengecek titik terhadap rect window lalu `PostMessage`. Selalu `CallNextHookEx`, jangan pernah memblok event.
- Hit-test tetap mengikuti geometri saat ini (`FillContainsPoint`); di luar bentuk → `HTTRANSPARENT`.

`--demo` tambahan: Space = play/pause, C = tutup sumber media palsu (untuk menguji grace dan clear pin).

---

## E. Nilai untuk M2 (dicatat sekarang, dipakai di M2)

| Konstanta | Nilai |
|---|---|
| `Timing::PauseGraceMs` | 30000 |
| `Timing::PausedMorphMs` | 200 (ease, dua arah) |
| `kPausedArtOpacity` | 0.5 |
| `kPausedDotHeight` | 3.0 |

Rect titik paused, `namespace Compact`, origin = pojok kiri-atas body Compact (240 × 34):

| Elemen | X | Y | W | H | r | `D2D1_RECT_F` {L, T, R, B} |
|---|--:|--:|--:|--:|--:|---|
| `Dot0` | 209 | 15.5 | 3 | 3 | 1.5 | `{209, 15.5, 212, 18.5}` |
| `Dot1` | 215 | 15.5 | 3 | 3 | 1.5 | `{215, 15.5, 218, 18.5}` |
| `Dot2` | 221 | 15.5 | 3 | 3 | 1.5 | `{221, 15.5, 224, 18.5}` |
| `Dot3` | 227 | 15.5 | 3 | 3 | 1.5 | `{227, 15.5, 230, 18.5}` |

Titik berbagi x dan pusat vertikal (y = 17) dengan wave bar `Bar0–Bar3`. Morph = tinggi bar diinterpolasi ke 3 DIP dalam 200 ms, animasi wave berhenti. Warna dan gradien sama dengan bar (Aqua → aksen).

## F. Timing lengkap (`namespace Timing`)

| Konstanta | ms |
|---|--:|
| `HoverDelayMsDefault` | 150 (ditimpa `config.hoverDelayMs`, clamp 0–1000) |
| `CollapseDelayMs` | 250 |
| `HudVisibleMs` | 1800 |
| `PauseGraceMs` | 30000 |
| `PausedMorphMs` | 200 |
| `ContentFadeInMs` | 240 |
| `ContentFadeDelayMs` | 70 |
| `ContentFadeOutMs` | 120 |
| `TelemetryPollMs` | 1000 |

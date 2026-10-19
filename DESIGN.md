# DESIGN.md — Notch Island

Design system dan spesifikasi visual untuk Notch Island: overlay Windows ala macOS Notch / Dynamic Island yang menempel di tepi atas layar. Dokumen ini adalah **sumber kebenaran tunggal** untuk semua angka. Kode tidak boleh punya angka visual yang tidak tercantum di sini.

Prototipe interaktif (referensi visual): artifact **Notch Island** di claude.ai. Semua tabel di bawah dihasilkan dari spec yang sama dengan prototipe.

**Versi 0.2** · Target: **Windows 11 x64 saja** (build 22000+). Tidak ada fallback Windows 10.

### Keputusan yang dikunci (v0.2)

| # | Keputusan |
|---|---|
| D1 | Hover **dan** klik didukung. Hover memicu Hover Swell seketika lalu Expanded setelah 150 ms. Klik kiri di Dormant/Compact langsung membuka Expanded dan mem-*pin*-nya. Klik tengah di Compact = play/pause global. |
| D2 | Saat media di-pause, Compact bertahan **30 detik**: bar membeku jadi titik 3 DIP, mini art 50% opacity. Lewat 30 detik, atau aplikasi media ditutup, kembali ke Dormant. |
| D3 | Konfigurasi pengguna di `%APPDATA%\NotchIsland\config.json` (via `SHGetKnownFolderPath(FOLDERID_RoamingAppData)`), dengan default hardcoded per field. |
| D4 | Hanya Windows 11 x64. |

---

## 1. Prinsip desain

1. **Menyatu dengan bezel.** Island duduk di `Y = 0` layar. Saat Dormant warnanya `#000000` murni tanpa rim dan tanpa shadow, jadi terlihat seperti bagian dari hardware.
2. **Tumbuh dari tepi, bukan muncul di atasnya.** Sudut atas memakai "liquid ears" (busur cekung). Sudut bawah memakai kurva kontinu (squircle), bukan busur lingkaran.
3. **Satu bentuk, satu pegas.** Semua perubahan state dianimasikan oleh satu set pegas yang menggerakkan lebar, tinggi, radius, warna permukaan, rim, dan shadow bersamaan. Outline tidak boleh "patah" di tengah animasi.
4. **Konten menyusul bentuk.** Konten state baru baru muncul setelah bentuk mulai stabil (fade + blur 240 ms, delay 70 ms).
5. **Tenang secara default.** Aksen Aqua/Sky hanya untuk hal yang aktif: audio yang diputar, progress, telemetri. Tidak ada warna dekoratif.
6. **Tidak pernah mencuri fokus.** Overlay tidak boleh mengambil keyboard focus, tidak muncul di taskbar, dan klik di luar bentuk island tembus ke aplikasi di bawahnya.

---

## 2. Unit dan sistem koordinat

- **Unit: DIP** (device-independent pixel). 1 DIP = 1 px pada 96 dpi (scaling 100%). Pada 150%, 1 DIP = 1.5 px.
- Panggil `ID2D1DeviceContext::SetDpi(dpi, dpi)` dengan nilai dari `GetDpiForWindow`, lalu pakai semua angka di dokumen ini apa adanya.
- **Origin lokal** = pojok kiri-atas **body** island. Ear kiri berada di `x ∈ [−e, 0]`, ear kanan di `x ∈ [W, W + e]`.
- **Origin layar island**: `left = floor((lebarMonitorDip − W) / 2)`, `top = 0` (tepi atas monitor, bukan work area, karena island menempel ke bezel).
- **`D2D1_RECT_F` = `{ left, top, right, bottom }`**, bukan `{ x, y, w, h }`. Semua tabel mencantumkan kedua bentuk.
- Setelah dikali DPI, bulatkan tepi rect ke piksel utuh agar rim 1 DIP tetap tajam.

---

## 3. State

| # | State | Kapan tampil |
|---|---|---|
| 1 | **Dormant** | Tidak ada aktivitas. Default saat idle. |
| 2 | **Compact media** | Sesi SMTC berstatus Playing, **atau** Paused dan masih dalam grace 30 detik (lihat 3.3). |
| 3 | **Expanded dashboard** | Kursor diam di atas island selama `hoverDelayMs` (default 150 ms), **atau** klik kiri (pinned). Punya 3 tab: Now Playing, Telemetry, Drop Tray. |
| 4 | **System HUD** | Volume / brightness berubah, atau timer Pomodoro selesai. Tampil 1800 ms lalu kembali ke state sebelumnya. |

Penomoran state tetap **1–4** di seluruh dokumen dan kode (Dormant = 1).

Prioritas jika bertabrakan: **HUD > Expanded (pinned atau hover) > Compact > Dormant**. HUD menggantikan Expanded sementara; status pin **tidak** hilang, jadi setelah HUD selesai island kembali ke Expanded jika masih pinned atau masih di-hover.

### 3.0 Mesin state

```
base      = Compact jika media Playing atau dalam grace pause, selain itu Dormant
expanded  = base ∈ {Dormant, Compact} && (pinned || (hovered && hoverDelay lewat && !suppressHover))
swell     = base ∈ {Dormant, Compact} && hovered && !expanded && !pinned && !suppressHover && !hud
tampil    = hud ? HUD : expanded ? Expanded : base
```

- `pinned` di-set oleh klik kiri, di-clear oleh klik kiri berikutnya di area kosong island, mouse-down di luar island, atau aplikasi media ditutup saat base Compact (base berubah jadi Dormant tidak meng-clear pin; hanya penutupan sumber media).
- `suppressHover` di-set saat unpin lewat klik ketika kursor masih di atas island, di-clear saat kursor keluar. Tanpa ini island langsung membuka lagi 150 ms setelah ditutup.

### 3.1 Geometri per state

| State | W | H | Bottom r | Ear r | Body `D2D1_RECT_F` | Screen rect @1920 | Surface | Rim | Shadow |
|---|--:|--:|--:|--:|---|---|---|--:|--:|
| 1 · Dormant | 160 | 28 | 12 | 8 | `{0, 0, 160, 28}` | `{880, 0, 1040, 28}` | Core | 0 | 0 |
| 2 · Compact media | 240 | 34 | 17 | 9 | `{0, 0, 240, 34}` | `{840, 0, 1080, 34}` | Core | 0.5 | 0.25 |
| 3 · Expanded dashboard | 400 | 145 | 28 | 14 | `{0, 0, 400, 145}` | `{760, 0, 1160, 145}` | Obsidian | 1 | 1 |
| 4 · System HUD | 200 | 44 | 22 | 10 | `{0, 0, 200, 44}` | `{860, 0, 1060, 44}` | Core | 0.75 | 0.4 |

- **Rim** dan **Shadow** adalah pengali 0–1 untuk token `Specular rim` dan `Drop shadow`.
- **Surface mix**: 0 = Core, 1 = Obsidian. Hanya Expanded yang bernilai 1.
- Ukuran window overlay tetap: **492 × 193 DIP** (envelope terbesar 428 × 145 + ruang shadow), di-anchor di tengah atas monitor pilihan (`monitorIndex`). Bentuk dianimasikan di dalam window; window **tidak** di-resize per frame.

### 3.2 Hover Swell

Bukan state tersendiri, melainkan **pengubah** target geometri Dormant/Compact selama kursor di atas island dan Expanded belum terbuka.

| Nilai | Delta | Dormant jadi | Compact jadi |
|---|--:|---|---|
| Width | +8 | 168 | 248 |
| Height | +2 | 30 | 36 |
| Bottom radius | +1 | 13 | 18 |
| Ear radius | 0 | 8 | 9 |

- Dimulai seketika saat kursor masuk, memakai pegas `HoverSwell`.
- Saat `hoverDelayMs` lewat, pegas `ToExpanded` mengambil alih dari posisi & velocity swell saat itu.
- Jika kursor keluar sebelum delay lewat, kembali dengan pegas `HoverSwell`.
- Konten tidak berubah; body tumbuh simetris dari tengah dan layer konten digeser `+dw / 2` (= 4 DIP) agar tetap di tengah. Lihat 5.1.
- Tidak ada swell di HUD, di Expanded, saat pinned, atau saat `suppressHover`.

### 3.3 Compact saat pause

| Hal | Playing | Paused (grace) |
|---|---|---|
| Wave bar | 4 bar animasi, h 4.48–16 | Membeku jadi titik **3 × 3 DIP**, r 1.5, pusat y = 17 (rect di Bagian 5) |
| Mini album art | opacity 1.0 | opacity **0.5** |
| Transisi | — | 200 ms ease (`PausedMorphMs`), dua arah |
| Status dot (Dormant) | opacity 0.85 | opacity 0.35 |

- Grace **30 000 ms** (`PauseGraceMs`) dihitung sejak event pause terakhir. Resume dalam grace membatalkan timer.
- Setelah grace habis: morph ke Dormant dengan pegas `CompactToIdle`.
- **Aplikasi media ditutup** (sesi SMTC hilang / `SessionsChanged` tanpa sesi aktif): langsung ke Dormant tanpa grace, dan pin di-clear.
- Resume dari Dormant (grace sudah lewat): Dormant → Compact dengan pegas `IdleToCompact`.

---

## 4. Konstruksi outline

```
mulai di (−e, 0)
garis ke (W + e, 0)                         ← tepi layar
arc radius e, pusat (W + e, e), ke (W, e)    ← ear kanan, CCW, small arc
garis ke (W, H − c)
cubic (W, H − k) (W − k, H) → (W − c, H)    ← sudut kanan bawah
garis ke (c, H)
cubic (k, H) (0, H − k) → (0, H − c)        ← sudut kiri bawah
garis ke (0, e)
arc radius e, pusat (−e, e), ke (−e, 0)      ← ear kiri, CCW, small arc
tutup
```

- `c = clamp(min(1.25 × r, W / 2, H − e), 0, ∞)` — jangkauan sudut.
- `k = 0.30 × c` — jarak handle Bézier dari titik sudut.
- Pada 45°, kurva ini berada dalam 0.004 r dari lingkaran radius r, tetapi kelengkungannya naik bertahap (continuous curvature).
- Clamp `H − e` mencegah kurva bawah menembus ear saat pegas overshoot.
- Jika `e == 0`, lewati kedua `AddArc` (arc dengan radius 0 tidak valid).
- Geometri dibangun ulang tiap frame animasi dari nilai pegas saat itu. Saat diam, cache geometrinya.

### 4.1 Permukaan, rim, shadow

| Lapisan | Aturan |
|---|---|
| Surface | `rgb = lerp(Core, Obsidian, mix)`, `a = lerp(1.0, 0.92, mix)`. Clamp `mix` ke [0, 1] (pegas bisa overshoot). |
| Rim | Linear gradient vertikal `y = 0 → H`, alpha `0.12 → 0.04`, dikali `rim`. Stroke 2 DIP pada geometri, di dalam `PushLayer` dengan geometric mask = island, sehingga yang terlihat hanya 1 DIP di sisi dalam. |
| Shadow | Offset (0, 12), blur 32 DIP, `#000000` alpha `0.55 × shadow`. Pakai effect `CLSID_D2D1Shadow` pada bitmap bentuk, atau pre-render saat diam. |
| Konten | Di-clip ke geometri island (`PushLayer` dengan geometric mask yang sama). |

---

## 5. Layout elemen

Semua koordinat dalam DIP, origin = pojok kiri-atas body. Elemen dengan `r > 0` digambar sebagai `D2D1_ROUNDED_RECT`.

### Dormant — `namespace Idle`

Screen X @1920 = local X + 880.

| Element | X | Y | W | H | r | `D2D1_RECT_F` {L,T,R,B} | Token | Catatan |
|---|--:|--:|--:|--:|--:|---|---|---|
| Island body | 0 | 0 | 160 | 28 | 12 | `{0, 0, 160, 28}` | Core | Bottom corners: continuous curve, see geometry recipe |
| Left ear (concave) | -8 | 0 | 8 | 8 | 8 | `{-8, 0, 0, 8}` | Core | Arc center (−e, e) |
| Right ear (concave) | 160 | 0 | 8 | 8 | 8 | `{160, 0, 168, 8}` | Core | Arc center (W + e, e) |
| Camera lens | 76 | 10 | 8 | 8 | 4 | `{76, 10, 84, 18}` | Camera lens | 1px stroke in Lens ring |
| Status dot | 138 | 11 | 6 | 6 | 3 | `{138, 11, 144, 17}` | Aqua | Opacity 0.85, sky glow |

### Compact media — `namespace Compact`

Screen X @1920 = local X + 840.

| Element | X | Y | W | H | r | `D2D1_RECT_F` {L,T,R,B} | Token | Catatan |
|---|--:|--:|--:|--:|--:|---|---|---|
| Island body | 0 | 0 | 240 | 34 | 17 | `{0, 0, 240, 34}` | Core | Bottom corners: continuous curve, see geometry recipe |
| Left ear (concave) | -9 | 0 | 9 | 9 | 9 | `{-9, 0, 0, 9}` | Core | Arc center (−e, e) |
| Right ear (concave) | 240 | 0 | 9 | 9 | 9 | `{240, 0, 249, 9}` | Core | Arc center (W + e, e) |
| Album art | 10 | 6 | 22 | 22 | 6 | `{10, 6, 32, 28}` | Label primary | Bitmap brush clipped to rounded rect |
| Waveform group | 209 | 9 | 21 | 16 | — | `{209, 9, 230, 25}` | Sky | Envelope of the 4 bars |
| Wave bar 1 | 209 | 9 | 3 | 16 | 1.5 | `{209, 9, 212, 25}` | Sky | Playing: height 4.48–16, centered on y = 17. Aqua → Sky gradient |
| Wave bar 2 | 215 | 9 | 3 | 16 | 1.5 | `{215, 9, 218, 25}` | Sky | Playing: height 4.48–16, centered on y = 17. Aqua → Sky gradient |
| Wave bar 3 | 221 | 9 | 3 | 16 | 1.5 | `{221, 9, 224, 25}` | Sky | Playing: height 4.48–16, centered on y = 17. Aqua → Sky gradient |
| Wave bar 4 | 227 | 9 | 3 | 16 | 1.5 | `{227, 9, 230, 25}` | Sky | Playing: height 4.48–16, centered on y = 17. Aqua → Sky gradient |
| Paused dot 1 | 209 | 15.5 | 3 | 3 | 1.5 | `{209, 15.5, 212, 18.5}` | Sky | Paused: bar frozen to a 3 × 3 dot, same center |
| Paused dot 2 | 215 | 15.5 | 3 | 3 | 1.5 | `{215, 15.5, 218, 18.5}` | Sky | Paused: bar frozen to a 3 × 3 dot, same center |
| Paused dot 3 | 221 | 15.5 | 3 | 3 | 1.5 | `{221, 15.5, 224, 18.5}` | Sky | Paused: bar frozen to a 3 × 3 dot, same center |
| Paused dot 4 | 227 | 15.5 | 3 | 3 | 1.5 | `{227, 15.5, 230, 18.5}` | Sky | Paused: bar frozen to a 3 × 3 dot, same center |

### Expanded · Now Playing — `namespace ExpandedNowPlaying`

Screen X @1920 = local X + 760.

| Element | X | Y | W | H | r | `D2D1_RECT_F` {L,T,R,B} | Token | Catatan |
|---|--:|--:|--:|--:|--:|---|---|---|
| Island body | 0 | 0 | 400 | 145 | 28 | `{0, 0, 400, 145}` | Obsidian | Bottom corners: continuous curve, see geometry recipe |
| Left ear (concave) | -14 | 0 | 14 | 14 | 14 | `{-14, 0, 0, 14}` | Obsidian | Arc center (−e, e) |
| Right ear (concave) | 400 | 0 | 14 | 14 | 14 | `{400, 0, 414, 14}` | Obsidian | Arc center (W + e, e) |
| Tab track | 16 | 12 | 252 | 22 | 11 | `{16, 12, 268, 34}` | Control fill | Segmented control background |
| Active tab pill | 18 | 14 | 80 | 18 | 9 | `{18, 14, 98, 32}` | Selected pill | x = 18 + 84 × tabIndex. Slides on tab change |
| Tab · Now Playing | 16 | 12 | 84 | 22 | — | `{16, 12, 100, 34}` | Label primary | Label centered, 11/600 |
| Tab · Telemetry | 100 | 12 | 84 | 22 | — | `{100, 12, 184, 34}` | Label primary | Label centered, 11/600 |
| Tab · Drop Tray | 184 | 12 | 84 | 22 | — | `{184, 12, 268, 34}` | Label primary | Label centered, 11/600 |
| Clock | 304 | 12 | 80 | 22 | — | `{304, 12, 384, 34}` | Label secondary | Right-aligned, 11/600 tabular |
| Album art | 16 | 46 | 64 | 64 | 12 | `{16, 46, 80, 110}` | Label primary | Bitmap brush clipped to rounded rect |
| Song title | 92 | 47 | 176 | 18 | — | `{92, 47, 268, 65}` | Label primary | Serif 15/600, trim with ellipsis |
| Artist | 92 | 66 | 176 | 14 | — | `{92, 66, 268, 80}` | Label secondary | 12/500, trim with ellipsis |
| Scrub hit area | 92 | 82 | 176 | 16 | — | `{92, 82, 268, 98}` | Control fill | Invisible. Pointer capture on press |
| Scrub track | 92 | 88 | 176 | 4 | 2 | `{92, 88, 268, 92}` | Track |  |
| Scrub fill | 92 | 88 | 176 | 4 | 2 | `{92, 88, 268, 92}` | Sky | Width = 176 × progress |
| Scrub knob | 87 | 85 | 10 | 10 | 5 | `{87, 85, 97, 95}` | Label primary | Center x = 92 + 176 × progress, y = 90 |
| Elapsed time | 92 | 96 | 40 | 11 | — | `{92, 96, 132, 107}` | Label tertiary | 10/500 tabular, left |
| Remaining time | 228 | 96 | 40 | 11 | — | `{228, 96, 268, 107}` | Label tertiary | 10/500 tabular, right |
| Previous | 130 | 112 | 24 | 24 | 12 | `{130, 112, 154, 136}` | Label primary | Glyph 12 DIP. Press: Selected pill fill, scale 0.88 |
| Play / Pause | 166 | 110 | 28 | 28 | 14 | `{166, 110, 194, 138}` | Label primary | Glyph 14 DIP |
| Next | 206 | 112 | 24 | 24 | 12 | `{206, 112, 230, 136}` | Label primary | Glyph 12 DIP |
| Column divider | 276 | 46 | 1 | 90 | — | `{276, 46, 277, 136}` | Control fill |  |
| CPU ring | 290 | 48 | 40 | 40 | 20 | `{290, 48, 330, 88}` | Sky | Stroke 3.5, arc starts at 12 o'clock. Value centered |
| RAM ring | 338 | 48 | 40 | 40 | 20 | `{338, 48, 378, 88}` | Sky | Stroke 3.5. Value centered |
| CPU label | 290 | 92 | 40 | 11 | — | `{290, 92, 330, 103}` | Label tertiary | Micro label, centered |
| RAM label | 338 | 92 | 40 | 11 | — | `{338, 92, 378, 103}` | Label tertiary | Micro label, centered |
| GPU label + value | 290 | 108 | 88 | 11 | — | `{290, 108, 378, 119}` | Label secondary | Label left, value right |
| GPU micro-pill | 290 | 123 | 88 | 4 | 2 | `{290, 123, 378, 127}` | Sky | Track + fill, width × load |

### Expanded · Telemetry — `namespace ExpandedTelemetry`

Screen X @1920 = local X + 760.

| Element | X | Y | W | H | r | `D2D1_RECT_F` {L,T,R,B} | Token | Catatan |
|---|--:|--:|--:|--:|--:|---|---|---|
| Island body | 0 | 0 | 400 | 145 | 28 | `{0, 0, 400, 145}` | Obsidian | Bottom corners: continuous curve, see geometry recipe |
| Left ear (concave) | -14 | 0 | 14 | 14 | 14 | `{-14, 0, 0, 14}` | Obsidian | Arc center (−e, e) |
| Right ear (concave) | 400 | 0 | 14 | 14 | 14 | `{400, 0, 414, 14}` | Obsidian | Arc center (W + e, e) |
| Tab track | 16 | 12 | 252 | 22 | 11 | `{16, 12, 268, 34}` | Control fill | Segmented control background |
| Active tab pill | 102 | 14 | 80 | 18 | 9 | `{102, 14, 182, 32}` | Selected pill | x = 18 + 84 × tabIndex. Slides on tab change |
| Tab · Now Playing | 16 | 12 | 84 | 22 | — | `{16, 12, 100, 34}` | Label primary | Label centered, 11/600 |
| Tab · Telemetry | 100 | 12 | 84 | 22 | — | `{100, 12, 184, 34}` | Label primary | Label centered, 11/600 |
| Tab · Drop Tray | 184 | 12 | 84 | 22 | — | `{184, 12, 268, 34}` | Label primary | Label centered, 11/600 |
| Clock | 304 | 12 | 80 | 22 | — | `{304, 12, 384, 34}` | Label secondary | Right-aligned, 11/600 tabular |
| CPU label | 16 | 46 | 120 | 12 | — | `{16, 46, 136, 58}` | Label secondary | Micro label |
| CPU value | 324 | 46 | 60 | 12 | — | `{324, 46, 384, 58}` | Label primary | 11/600 tabular, right |
| CPU bar | 16 | 61 | 368 | 4 | 2 | `{16, 61, 384, 65}` | Sky | Track + fill |
| RAM label | 16 | 68 | 120 | 12 | — | `{16, 68, 136, 80}` | Label secondary | Micro label |
| RAM value | 324 | 68 | 60 | 12 | — | `{324, 68, 384, 80}` | Label primary | 11/600 tabular, right |
| RAM bar | 16 | 83 | 368 | 4 | 2 | `{16, 83, 384, 87}` | Sky | Track + fill |
| GPU label | 16 | 90 | 120 | 12 | — | `{16, 90, 136, 102}` | Label secondary | Micro label |
| GPU value | 324 | 90 | 60 | 12 | — | `{324, 90, 384, 102}` | Label primary | 11/600 tabular, right |
| GPU bar | 16 | 105 | 368 | 4 | 2 | `{16, 105, 384, 109}` | Sky | Track + fill |
| NET label | 16 | 112 | 120 | 12 | — | `{16, 112, 136, 124}` | Label secondary | Micro label |
| NET value | 324 | 112 | 60 | 12 | — | `{324, 112, 384, 124}` | Label primary | 11/600 tabular, right |
| NET bar | 16 | 127 | 368 | 4 | 2 | `{16, 127, 384, 131}` | Sky | Track + fill |

### Expanded · Drop Tray — `namespace ExpandedDropTray`

Screen X @1920 = local X + 760.

| Element | X | Y | W | H | r | `D2D1_RECT_F` {L,T,R,B} | Token | Catatan |
|---|--:|--:|--:|--:|--:|---|---|---|
| Island body | 0 | 0 | 400 | 145 | 28 | `{0, 0, 400, 145}` | Obsidian | Bottom corners: continuous curve, see geometry recipe |
| Left ear (concave) | -14 | 0 | 14 | 14 | 14 | `{-14, 0, 0, 14}` | Obsidian | Arc center (−e, e) |
| Right ear (concave) | 400 | 0 | 14 | 14 | 14 | `{400, 0, 414, 14}` | Obsidian | Arc center (W + e, e) |
| Tab track | 16 | 12 | 252 | 22 | 11 | `{16, 12, 268, 34}` | Control fill | Segmented control background |
| Active tab pill | 186 | 14 | 80 | 18 | 9 | `{186, 14, 266, 32}` | Selected pill | x = 18 + 84 × tabIndex. Slides on tab change |
| Tab · Now Playing | 16 | 12 | 84 | 22 | — | `{16, 12, 100, 34}` | Label primary | Label centered, 11/600 |
| Tab · Telemetry | 100 | 12 | 84 | 22 | — | `{100, 12, 184, 34}` | Label primary | Label centered, 11/600 |
| Tab · Drop Tray | 184 | 12 | 84 | 22 | — | `{184, 12, 268, 34}` | Label primary | Label centered, 11/600 |
| Clock | 304 | 12 | 80 | 22 | — | `{304, 12, 384, 34}` | Label secondary | Right-aligned, 11/600 tabular |
| Drop zone | 16 | 46 | 368 | 88 | 16 | `{16, 46, 384, 134}` | Control fill | Dashed 1px Label tertiary, dash 4 / gap 3 |
| File tile 1 | 29 | 58 | 106 | 64 | 10 | `{29, 58, 135, 122}` | Control fill | Icon 20 at (12, 12) · name 11/600 at (12, 40) |
| File tile 2 | 147 | 58 | 106 | 64 | 10 | `{147, 58, 253, 122}` | Control fill | Icon 20 at (12, 12) · name 11/600 at (12, 40) |
| File tile 3 | 265 | 58 | 106 | 64 | 10 | `{265, 58, 371, 122}` | Control fill | Icon 20 at (12, 12) · name 11/600 at (12, 40) |

### HUD · Volume / Brightness — `namespace HudLevel`

Screen X @1920 = local X + 860.

| Element | X | Y | W | H | r | `D2D1_RECT_F` {L,T,R,B} | Token | Catatan |
|---|--:|--:|--:|--:|--:|---|---|---|
| Island body | 0 | 0 | 200 | 44 | 22 | `{0, 0, 200, 44}` | Core | Bottom corners: continuous curve, see geometry recipe |
| Left ear (concave) | -10 | 0 | 10 | 10 | 10 | `{-10, 0, 0, 10}` | Core | Arc center (−e, e) |
| Right ear (concave) | 200 | 0 | 10 | 10 | 10 | `{200, 0, 210, 10}` | Core | Arc center (W + e, e) |
| Icon | 14 | 12 | 20 | 20 | — | `{14, 12, 34, 32}` | Label primary | Speaker or sun glyph |
| Level track | 44 | 20 | 112 | 4 | 2 | `{44, 20, 156, 24}` | Track |  |
| Level fill | 44 | 20 | 112 | 4 | 2 | `{44, 20, 156, 24}` | Sky | Width = 112 × level |
| Level value | 160 | 14 | 26 | 16 | — | `{160, 14, 186, 30}` | Label primary | 13/600 tabular, right |

### HUD · Pomodoro — `namespace HudPomodoro`

Screen X @1920 = local X + 860.

| Element | X | Y | W | H | r | `D2D1_RECT_F` {L,T,R,B} | Token | Catatan |
|---|--:|--:|--:|--:|--:|---|---|---|
| Island body | 0 | 0 | 200 | 44 | 22 | `{0, 0, 200, 44}` | Core | Bottom corners: continuous curve, see geometry recipe |
| Left ear (concave) | -10 | 0 | 10 | 10 | 10 | `{-10, 0, 0, 10}` | Core | Arc center (−e, e) |
| Right ear (concave) | 200 | 0 | 10 | 10 | 10 | `{200, 0, 210, 10}` | Core | Arc center (W + e, e) |
| Timer ring | 14 | 12 | 20 | 20 | 10 | `{14, 12, 34, 32}` | Focus orange | Stroke 2.5, remaining fraction |
| Session label | 44 | 14 | 90 | 16 | — | `{44, 14, 134, 30}` | Label primary | 13/600 |
| Time left | 136 | 14 | 50 | 16 | — | `{136, 14, 186, 30}` | Focus orange | 13/600 tabular, right |

### 5.1 Elemen dinamis

| Elemen | Rumus |
|---|---|
| Active tab pill | `x = 18 + 84 × tabIndex`. Animasi geser memakai pegas `ToExpanded`. |
| Scrub fill | `width = 176 × progress` |
| Scrub knob | pusat `(92 + 176 × progress, 90)`, ukuran 10 × 10 |
| Wave bar i | `x = 209 + 6i`, `w = 3`, `h ∈ [4.48, 16]` saat playing, `h = 3` saat paused, pusat vertikal `y = 17` |
| Origin layer konten | `x = (W_saat_ini − W_target) / 2`. Konten selalu di tengah body selama animasi dan swell (swell: +4 DIP). Semua rect Bagian 5 relatif ke origin ini. |
| Level fill (HUD) | `width = 112 × level` |
| Telemetry bar | `width = 368 × value` (tab Telemetry), `88 × value` (GPU pill) |
| Ring | stroke 3.5 (telemetri) atau 2.5 (Pomodoro), radius `(w − stroke) / 2`, mulai jam 12, searah jarum jam, cap round |

---

## 6. Warna

Alpha di bawah adalah **straight alpha**. Jika render ke swap chain `DXGI_ALPHA_MODE_PREMULTIPLIED`, Direct2D brush menerima straight alpha dan mengurus premultiply sendiri; yang perlu di-premultiply manual hanya warna yang ditulis langsung ke buffer.

| Token | Hex | Alpha | CSS | `D2D1_COLOR_F` | Pemakaian |
|---|---|--:|---|---|---|
| Core (`Color::Core`) | `#000000` | 1.00 | `rgba(0, 0, 0, 1)` | `{ 0.000f, 0.000f, 0.000f, 1.000f }` | Surface for Dormant, Compact and HUD. Matches the bezel. |
| Obsidian (`Color::Obsidian`) | `#0C0C0E` | 0.92 | `rgba(12, 12, 14, 0.92)` | `{ 0.047f, 0.047f, 0.055f, 0.920f }` | Surface for Expanded. Interpolated from Core by the "mix" spring. |
| Specular rim (`Color::Rim`) | `#FFFFFF` | 0.12 | `rgba(255, 255, 255, 0.12)` | `{ 1.000f, 1.000f, 1.000f, 0.120f }` | 1px inner border. Alpha 0.12 at y = 0 fading to 0.04 at y = H. Scaled by the "rim" spring. |
| Aqua (`Color::Aqua`) | `#00FFFF` | 1.00 | `rgba(0, 255, 255, 1)` | `{ 0.000f, 1.000f, 1.000f, 1.000f }` | Accent gradient start (top of bars, status dot). |
| Sky (`Color::Sky`) | `#38BDF8` | 1.00 | `rgba(56, 189, 248, 1)` | `{ 0.220f, 0.741f, 0.973f, 1.000f }` | Primary accent: progress fills, rings, gradient end. |
| Sky glow (`Color::Glow`) | `#38BDF8` | 0.45 | `rgba(56, 189, 248, 0.45)` | `{ 0.220f, 0.741f, 0.973f, 0.450f }` | Glow behind accent marks. Gaussian blur σ ≈ 3 DIP. |
| Label primary (`Color::Label`) | `#F5F5F7` | 1.00 | `rgba(245, 245, 247, 1)` | `{ 0.961f, 0.961f, 0.969f, 1.000f }` | Titles, values, glyphs. |
| Label secondary (`Color::Label2`) | `#EBEBF5` | 0.60 | `rgba(235, 235, 245, 0.6)` | `{ 0.922f, 0.922f, 0.961f, 0.600f }` | Artist, clock, secondary values. |
| Label tertiary (`Color::Label3`) | `#EBEBF5` | 0.30 | `rgba(235, 235, 245, 0.3)` | `{ 0.922f, 0.922f, 0.961f, 0.300f }` | Micro labels, timestamps, dashed tray stroke. |
| Control fill (`Color::Fill`) | `#FFFFFF` | 0.08 | `rgba(255, 255, 255, 0.08)` | `{ 1.000f, 1.000f, 1.000f, 0.080f }` | Segmented track, tray tiles, divider. |
| Selected pill (`Color::Pill`) | `#FFFFFF` | 0.16 | `rgba(255, 255, 255, 0.16)` | `{ 1.000f, 1.000f, 1.000f, 0.160f }` | Active tab pill, pressed button. |
| Track (`Color::Track`) | `#FFFFFF` | 0.14 | `rgba(255, 255, 255, 0.14)` | `{ 1.000f, 1.000f, 1.000f, 0.140f }` | Progress and ring tracks. |
| Camera lens (`Color::Lens`) | `#0A0A12` | 1.00 | `rgba(10, 10, 18, 1)` | `{ 0.039f, 0.039f, 0.071f, 1.000f }` | Dormant camera glyph fill. |
| Lens ring (`Color::LensRing`) | `#1C1C26` | 1.00 | `rgba(28, 28, 38, 1)` | `{ 0.110f, 0.110f, 0.149f, 1.000f }` | Dormant camera glyph 1px stroke. |
| Focus orange (`Color::Focus`) | `#FF9F0A` | 1.00 | `rgba(255, 159, 10, 1)` | `{ 1.000f, 0.624f, 0.039f, 1.000f }` | Pomodoro ring and time. |
| Drop shadow (`Color::Shadow`) | `#000000` | 0.55 | `rgba(0, 0, 0, 0.55)` | `{ 0.000f, 0.000f, 0.000f, 0.550f }` | Offset (0, 12), blur 32. Scaled by the "shadow" spring. |

- Gradien aksen: **Aqua → Sky** (horizontal untuk bar progres, vertikal atas→bawah untuk wave bar).
- Glow: drop shadow warna `Sky glow`, blur ≈ 3 DIP (σ), offset 0. Hanya pada elemen aksen yang aktif.
- **Aksen dari config.** `accentColor` menggantikan **Sky**. Ujung awal gradien:
  - jika `accentColor` = default `#38BDF8` → tetap **Aqua `#00FFFF`** (pasangan desain asli),
  - selain itu → `accent + (1 − accent) × 0.35` per kanal (dicampur 35% putih). Lihat `AccentStart()`.
  - Glow = `accentColor` dengan alpha 0.45.
  - Focus orange, label, surface, dan rim **tidak** ikut berubah.
- Kontras: semua teks Label primary/secondary di atas Core/Obsidian lolos WCAG AA. Label tertiary hanya untuk label mikro dan timestamp, bukan informasi penting.

---

## 7. Tipografi

| Role | macOS | Windows (DirectWrite) | Size | Weight | Line | Tracking | Color |
|---|---|---|--:|--:|--:|---|---|
| Tab label / clock | SF Pro Text | Segoe UI Variable Text | 11 | 600 | 14 | +0.1 | Label primary |
| Media title | New York | Georgia (titleFont serif) · Segoe UI Variable Display (sans) | 15 | 600 | 18 | -0.2 | Label primary |
| Artist | SF Pro Text | Segoe UI Variable Text | 12 | 500 | 14 | 0 | Label secondary |
| Timestamp | SF Pro Text | Segoe UI Variable Text | 10 | 500 | 11 | 0 · tabular | Label tertiary |
| Telemetry value | SF Pro Display | Segoe UI Variable Display | 11 | 600 | 12 | 0 · tabular | Label primary |
| Micro label | SF Pro Text | Segoe UI Variable Small | 9.5 | 600 | 11 | +0.6 · caps | Label tertiary |
| HUD value | SF Pro Display | Segoe UI Variable Display | 13 | 600 | 16 | 0 · tabular | Label primary |

- **Jangan bundle SF Pro atau New York.** Lisensinya hanya untuk platform Apple. Font UI: **Segoe UI Variable** (selalu ada di Windows 11, tidak perlu fallback). Opsional: bundle **Inter** (OFL) dan muat lewat `IDWriteFactory5::CreateInMemoryFontFileLoader`.
- Judul lagu mengikuti `titleFont` di config: `"serif"` (default) → **Georgia** 15/600; `"sans"` → **Segoe UI Variable Display** 15/600. Ukuran, line height, dan rect sama.
- Angka yang berubah (jam, persentase, waktu) wajib **tabular figures**: `DWRITE_FONT_FEATURE_TAG_TABULAR_FIGURES`.
- Antialias teks: `D2D1_TEXT_ANTIALIAS_MODE_GRAYSCALE`. ClearType tidak bekerja di permukaan transparan.
- Teks yang terlalu panjang: trimming `DWRITE_TRIMMING_GRANULARITY_CHARACTER` dengan ellipsis sign.
- Tracking dalam tabel adalah DIP (letter-spacing). DirectWrite tidak punya tracking langsung: pakai `IDWriteTextLayout1::SetCharacterSpacing`.

---

## 8. Motion

### 8.1 Parameter pegas

| Transisi | `Spring::` | Stiffness k | Damping c | Mass | Response | ζ | Overshoot | Settle |
|---|---|--:|--:|--:|--:|--:|--:|--:|
| Dormant → Compact | `IdleToCompact` | 223.8 | 23.94 | 1.0 | 0.42 s | 0.8 | 1.4% | 478 ms |
| Dormant / Compact → Expanded | `ToExpanded` | 171.3 | 19.37 | 1.0 | 0.48 s | 0.74 | 3.0% | 544 ms |
| Expanded → Dormant / Compact | `Collapse` | 304.6 | 32.11 | 1.0 | 0.36 s | 0.92 | 0.0% | 340 ms |
| Compact → Dormant | `CompactToIdle` | 341.5 | 35.11 | 1.0 | 0.34 s | 0.95 | 0.0% | 355 ms |
| Any → HUD | `ToHud` | 438.6 | 28.48 | 1.0 | 0.3 s | 0.68 | 5.2% | 327 ms |
| HUD → previous state | `FromHud` | 273.4 | 29.10 | 1.0 | 0.38 s | 0.88 | 0.2% | 313 ms |
| Hover swell in / out | `HoverSwell` | 503.6 | 27.83 | 1.0 | 0.28 s | 0.62 | 8.1% | 392 ms |

- Rumus: `k = (2π / response)² × m`, `c = 4π × ζ × m / response`, `m = 1`.
- **Settle** = waktu terakhir nilai menyimpang > 0.5% dari target.
- Integrator: **semi-implicit Euler, fixed step 1/240 s**, akumulasikan waktu frame nyata (maksimum 1/20 s per frame agar tidak meledak setelah sleep).
- Nilai yang dipegas bersama: `width, height, bottomRadius, earRadius, surfaceMix, rim, shadow`.
- Transisi yang terputus di tengah (mis. hover lalu cepat keluar) **meneruskan velocity** saat ini ke target baru. Jangan reset ke 0.
- Animasi berhenti saat semua nilai `|x − target| < 0.01` dan `|v| < 0.01`. Setelah itu stop render loop (hemat CPU/GPU).

### 8.2 Timing

| `Timing::` | ms | Aturan |
|---|--:|---|
| `HoverDelayMsDefault` | 150 | Hover → Expanded. Swell starts at 0 ms. Overridable by config hoverDelayMs (0–1000). |
| `CollapseDelayMs` | 250 | Cursor leaves an unpinned Expanded → collapse. Re-entering within this window cancels it. |
| `HudVisibleMs` | 1800 | Since the last HUD event. New events reset it. |
| `PauseGraceMs` | 30000 | Compact stays visible while paused, then morphs to Dormant. Closing the media app skips the wait. |
| `PausedMorphMs` | 200 | Bars → 3 DIP dots, mini art → 50% opacity (and back on resume). |
| `ContentFadeInMs` | 240 | Opacity 0 → 1, blur 3 → 0 DIP, cubic-bezier(0.2, 0.8, 0.2, 1). |
| `ContentFadeDelayMs` | 70 | Before content fade-in starts. |
| `ContentFadeOutMs` | 120 | Linear, starts with the spring. |
| `TelemetryPollMs` | 1000 | Only while a telemetry view is visible. |

| Hal lain | Nilai |
|---|---|
| Wave bar | Siklus 820 / 640 / 1040 / 760 ms, tinggi 4.48 ↔ 16 DIP (scale 0.28 ↔ 1.0, `Motion::WaveMinimumScale`), ease-in-out |
| Tekan tombol media | scale 0.88 + fill `Selected pill`, 120 ms |
| Perubahan nilai telemetri | dianimasikan 500 ms ease-out |

### 8.3 Reduced motion

Jika `SPI_GETCLIENTAREAANIMATION` = FALSE: lompati pegas (snap ke target), tidak ada Hover Swell, wave bar statis (tinggi 10 DIP, `Motion::ReducedMotionWaveHeight`, pusat y = 17), fade jadi 100 ms tanpa blur (`Motion::ReducedFadeMs`). Logika delay hover, pin, dan grace tetap sama.

---

## 9. Interaksi

| Input / event | Saat | Hasil |
|---|---|---|
| Kursor masuk | Dormant, Compact | Hover Swell seketika. Setelah `hoverDelayMs` → Expanded (tab terakhir). |
| Kursor keluar | Expanded tidak pinned | Collapse ke base setelah 250 ms. Masuk lagi dalam 250 ms membatalkannya. |
| Kursor keluar | Expanded pinned | Tidak terjadi apa-apa. |
| Klik kiri | Dormant, Compact | Lewati delay, langsung Expanded **dan pinned**. |
| Klik kiri di area kosong | Expanded (hover) | Pin. |
| Klik kiri di area kosong | Expanded (pinned) | Unpin, collapse seketika, set `suppressHover`. |
| Klik kiri di kontrol (tab, tombol, scrub, tile) | Expanded | Kontrol bekerja. Status pin tidak berubah. |
| Mouse down di luar island | Pinned | Unpin dan collapse. Klik tetap diteruskan ke aplikasi di bawah. |
| Klik tengah | Compact | Play/pause global (SMTC `TryTogglePlayPauseAsync`). Tidak expand, tidak mengubah swell. |
| Klik tengah | State lain | Diabaikan. |
| Klik kanan | Semua | Diabaikan di v1 (cadangan untuk menu konteks). |
| Klik tab | Expanded | Pindah tab, pill bergeser. |
| Klik Prev / Play-Pause / Next | Expanded | Kirim ke sesi SMTC aktif. |
| Drag di scrub | Expanded | Pointer capture, seek saat dilepas. |
| Drag file ke island | Semua kecuali HUD | Expanded terbuka di tab Drop Tray dan pinned selama drag. |
| Klik di luar bentuk island | Tidak pinned | Tembus ke aplikasi di bawah (`HTTRANSPARENT`). |
| Aplikasi fullscreen di foreground | Semua | Island disembunyikan (kecuali HUD), pin di-clear. |

- Area hit-test = geometri island saat ini (`ID2D1Geometry::FillContainsPoint`), bukan bounding box window.
- "Area kosong" = titik di dalam geometri yang tidak berada di rect kontrol mana pun di Bagian 5.
- Deteksi klik di luar island: karena window `WS_EX_NOACTIVATE` tidak menerima klik di luar bentuknya, pasang `WH_MOUSE_LL` **hanya selama pinned** dan lepas saat unpin. Hook tidak boleh memblok event (selalu `CallNextHookEx`).

---

## 10. Konfigurasi pengguna

Lokasi: `%APPDATA%\NotchIsland\config.json`, path dari `SHGetKnownFolderPath(FOLDERID_RoamingAppData)`.

```json
{
  "version": 1,
  "titleFont": "serif",
  "accentColor": "#38BDF8",
  "hoverDelayMs": 150,
  "monitorIndex": 0
}
```

| Field | Tipe | Default | Valid | Jika tidak valid |
|---|---|---|---|---|
| `version` | int | 1 | 1 | Anggap 1 |
| `titleFont` | string | `"serif"` | `"serif"`, `"sans"` | `"serif"` |
| `accentColor` | string | `"#38BDF8"` | `#RRGGBB` | `"#38BDF8"` |
| `hoverDelayMs` | int | 150 | 0–1000 | Clamp ke rentang; non-angka → 150 |
| `monitorIndex` | int | 0 | 0 = primary, 1..n = monitor lain diurutkan dari tepi kiri | Di luar rentang → 0 |

- File tidak ada → pakai default dan tulis file dengan default sekali (buat folder jika perlu).
- JSON rusak → pakai semua default, **jangan** menimpa file pengguna; catat warning ke log.
- Field tidak valid → hanya field itu yang memakai default.
- Field tidak dikenal → diabaikan dan dibiarkan di file.
- Dimuat saat start dan saat "Reload config" di menu tray. Tidak ada hot-reload otomatis di v1.
- Default dikunci di `kDefaultConfig` (Lampiran A).

---

## 11. Checklist verifikasi visual

- [ ] Hover Swell terlihat dalam frame pertama setelah kursor masuk; Expanded mulai tepat setelah `hoverDelayMs`.
- [ ] Klik kiri di Dormant langsung membuka Expanded tanpa menunggu delay, dan tetap terbuka saat kursor keluar.
- [ ] Klik tengah di Compact mem-pause musik tanpa membuka island.
- [ ] Saat pause: titik 3 DIP dan art 50% muncul dalam 200 ms; island kembali ke Dormant tepat 30 detik kemudian.
- [ ] Pada 100% dan 150% scaling, screenshot island cocok dengan prototipe (overlay di atas screenshot prototipe zoom 1×/1.5×, selisih ≤ 1 px).
- [ ] Debug overlay (toggle `Ctrl+Alt+F12`) menggambar semua rect di Bagian 5 dengan stroke 1 DIP Aqua, dan semua elemen jatuh tepat di dalamnya. FPS/IDLE ditoggle terpisah dengan `Ctrl+Shift+F`; keduanya default mati (Lampiran B).
- [ ] Dormant tidak punya rim, tidak punya shadow, dan tidak terlihat garis di tepi atas layar.
- [ ] Saat overshoot `ToHud` (5.2%), sudut bawah tidak menembus ear.
- [ ] Angka jam/persentase tidak "bergoyang" lebarnya (tabular figures aktif).

---

## Lampiran A — `NotchIslandSpec.h`

Header ini adalah terjemahan langsung dari tabel di atas. Taruh di `src/spec/NotchIslandSpec.h`. Jika ada perubahan desain, ubah dokumen ini dan header ini bersamaan.

```cpp
// Notch Island — generated from the design system sheet.
// Units are DIPs. Multiply by (dpi / 96.0f) only if you draw in raw pixels;
// ID2D1RenderTarget::SetDpi() does this for you.
// Local space: origin = top-left of the island BODY. Ears sit at x = -ear and x = width.
// D2D1_RECT_F is { left, top, right, bottom } — NOT { x, y, w, h }.
#pragma once
#include <d2d1.h>
#include <dwrite.h>
#include <array>
#include <algorithm>
#include <cmath>

namespace NotchIsland {

struct IslandGeom { float width, height, bottomRadius, earRadius, surfaceMix, rim, shadow; };
constexpr IslandGeom kGeomIdle { 160.0f, 28.0f, 12.0f, 8.0f, 0.0f, 0.0f, 0.0f };
constexpr IslandGeom kGeomCompact { 240.0f, 34.0f, 17.0f, 9.0f, 0.0f, 0.5f, 0.25f };
constexpr IslandGeom kGeomExpanded { 400.0f, 145.0f, 28.0f, 14.0f, 1.0f, 1.0f, 1.0f };
constexpr IslandGeom kGeomHud { 200.0f, 44.0f, 22.0f, 10.0f, 0.0f, 0.75f, 0.4f };

// Screen anchor: left = floor((monitorWidthDip - geom.width) / 2), top = monitor top edge (0).

// Restored M0 tokens (DESIGN Lampiran B). The fixed window realizes the screen anchor above:
// every geom width is even, so window left + (Width - geom.width) / 2 lands on the same DIP.
namespace WindowSpec {
  constexpr float Width = 492.0f;
  constexpr float Height = 193.0f;
  constexpr float Top = 0.0f;
  constexpr float Half = 0.5f;
  constexpr float DpiBase = 96.0f;
}
// Window x relative to rcMonitor.left: floor((monitorWidthDip - WindowSpec::Width) * WindowSpec::Half).
// Convert that offset to physical pixels; screen y = rcMonitor.top (never rcWork.top).
// Body origin in window DIPs: {windowCenterX - geom.width * WindowSpec::Half, WindowSpec::Top}.
namespace PaintSpec {
  constexpr float Zero = 0.0f;
  constexpr float Unit = 1.0f;
  constexpr float LensStroke = 1.0f;
  constexpr float DebugStroke = 1.0f;
  constexpr float RimStroke = 2.0f; // Geometric mask leaves 1 DIP inside.
  constexpr float RimBottomAlpha = 0.04f;
  constexpr float GlowSigma = 3.0f;
  constexpr D2D1_POINT_2F GlowOffset = { Zero, Zero };
  constexpr D2D1_POINT_2F ShadowOffset = { Zero, 12.0f };
  constexpr float ShadowBlur = 32.0f;
  constexpr float DotOpacityActive = 0.85f;
  constexpr float DotOpacityIdle = 0.35f;
}

// Hover Swell: added to Dormant / Compact targets while hovered and not yet expanded.
constexpr float kSwellDeltaWidth        = 8.0f;
constexpr float kSwellDeltaHeight       = 2.0f;
constexpr float kSwellDeltaBottomRadius = 1.0f;

// Paused Compact
constexpr float kPausedArtOpacity = 0.5f;
constexpr float kPausedDotHeight  = 3.0f;

namespace Timing { // milliseconds
  constexpr unsigned HoverDelayMsDefault = 150;
  constexpr unsigned CollapseDelayMs = 250;
  constexpr unsigned HudVisibleMs = 1800;
  constexpr unsigned PauseGraceMs = 30000;
  constexpr unsigned PausedMorphMs = 200;
  constexpr unsigned ContentFadeInMs = 240;
  constexpr unsigned ContentFadeDelayMs = 70;
  constexpr unsigned ContentFadeOutMs = 120;
  constexpr unsigned TelemetryPollMs = 1000;
}

namespace Idle {
  constexpr D2D1_RECT_F Body = { WindowSpec::Top, WindowSpec::Top, kGeomIdle.width, kGeomIdle.height };
  constexpr D2D1_RECT_F LeftEar = { -kGeomIdle.earRadius, WindowSpec::Top, WindowSpec::Top, kGeomIdle.earRadius };
  constexpr D2D1_RECT_F RightEar = { kGeomIdle.width, WindowSpec::Top, kGeomIdle.width + kGeomIdle.earRadius, kGeomIdle.earRadius };
  constexpr D2D1_ROUNDED_RECT Lens = { { 76.0f, 10.0f, 84.0f, 18.0f }, 4.0f, 4.0f }; // Camera lens
  constexpr D2D1_ROUNDED_RECT Status = { { 138.0f, 11.0f, 144.0f, 17.0f }, 3.0f, 3.0f }; // Status dot
}
namespace Compact {
  constexpr D2D1_ROUNDED_RECT Art = { { 10.0f, 6.0f, 32.0f, 28.0f }, 6.0f, 6.0f }; // Album art
  constexpr D2D1_RECT_F Wave = { 209.0f, 9.0f, 230.0f, 25.0f }; // Waveform group; playing bars are 4.48–16 DIP high, centered on y = 17.
  constexpr D2D1_ROUNDED_RECT Bar0 = { { 209.0f, 9.0f, 212.0f, 25.0f }, 1.5f, 1.5f }; // Wave bar 1
  constexpr D2D1_ROUNDED_RECT Bar1 = { { 215.0f, 9.0f, 218.0f, 25.0f }, 1.5f, 1.5f }; // Wave bar 2
  constexpr D2D1_ROUNDED_RECT Bar2 = { { 221.0f, 9.0f, 224.0f, 25.0f }, 1.5f, 1.5f }; // Wave bar 3
  constexpr D2D1_ROUNDED_RECT Bar3 = { { 227.0f, 9.0f, 230.0f, 25.0f }, 1.5f, 1.5f }; // Wave bar 4
  constexpr D2D1_ROUNDED_RECT Dot0 = { { 209.0f, 15.5f, 212.0f, 18.5f }, 1.5f, 1.5f }; // Paused dot 1
  constexpr D2D1_ROUNDED_RECT Dot1 = { { 215.0f, 15.5f, 218.0f, 18.5f }, 1.5f, 1.5f }; // Paused dot 2
  constexpr D2D1_ROUNDED_RECT Dot2 = { { 221.0f, 15.5f, 224.0f, 18.5f }, 1.5f, 1.5f }; // Paused dot 3
  constexpr D2D1_ROUNDED_RECT Dot3 = { { 227.0f, 15.5f, 230.0f, 18.5f }, 1.5f, 1.5f }; // Paused dot 4
}
namespace ExpandedNowPlaying {
  constexpr D2D1_ROUNDED_RECT TabTrack = { { 16.0f, 12.0f, 268.0f, 34.0f }, 11.0f, 11.0f }; // Tab track
  constexpr D2D1_ROUNDED_RECT TabSel = { { 18.0f, 14.0f, 98.0f, 32.0f }, 9.0f, 9.0f }; // Active tab pill
  constexpr D2D1_RECT_F Tab0 = { 16.0f, 12.0f, 100.0f, 34.0f }; // Tab · Now Playing
  constexpr D2D1_RECT_F Tab1 = { 100.0f, 12.0f, 184.0f, 34.0f }; // Tab · Telemetry
  constexpr D2D1_RECT_F Tab2 = { 184.0f, 12.0f, 268.0f, 34.0f }; // Tab · Drop Tray
  constexpr D2D1_RECT_F Clock = { 304.0f, 12.0f, 384.0f, 34.0f }; // Clock
  constexpr D2D1_ROUNDED_RECT Art = { { 16.0f, 46.0f, 80.0f, 110.0f }, 12.0f, 12.0f }; // Album art
  constexpr D2D1_RECT_F Title = { 92.0f, 47.0f, 268.0f, 65.0f }; // Song title
  constexpr D2D1_RECT_F Artist = { 92.0f, 66.0f, 268.0f, 80.0f }; // Artist
  constexpr D2D1_RECT_F ScrubHit = { 92.0f, 82.0f, 268.0f, 98.0f }; // Scrub hit area
  constexpr D2D1_ROUNDED_RECT ScrubTrack = { { 92.0f, 88.0f, 268.0f, 92.0f }, 2.0f, 2.0f }; // Scrub track
  constexpr D2D1_ROUNDED_RECT ScrubFill = { { 92.0f, 88.0f, 268.0f, 92.0f }, 2.0f, 2.0f }; // Scrub fill
  constexpr D2D1_ROUNDED_RECT Knob = { { 87.0f, 85.0f, 97.0f, 95.0f }, 5.0f, 5.0f }; // Scrub knob
  constexpr D2D1_RECT_F Elapsed = { 92.0f, 96.0f, 132.0f, 107.0f }; // Elapsed time
  constexpr D2D1_RECT_F Remain = { 228.0f, 96.0f, 268.0f, 107.0f }; // Remaining time
  constexpr D2D1_ROUNDED_RECT Prev = { { 130.0f, 112.0f, 154.0f, 136.0f }, 12.0f, 12.0f }; // Previous
  constexpr D2D1_ROUNDED_RECT Play = { { 166.0f, 110.0f, 194.0f, 138.0f }, 14.0f, 14.0f }; // Play / Pause
  constexpr D2D1_ROUNDED_RECT Next = { { 206.0f, 112.0f, 230.0f, 136.0f }, 12.0f, 12.0f }; // Next
  constexpr D2D1_RECT_F Divider = { 276.0f, 46.0f, 277.0f, 136.0f }; // Column divider
  constexpr D2D1_ROUNDED_RECT CpuRing = { { 290.0f, 48.0f, 330.0f, 88.0f }, 20.0f, 20.0f }; // CPU ring
  constexpr D2D1_ROUNDED_RECT RamRing = { { 338.0f, 48.0f, 378.0f, 88.0f }, 20.0f, 20.0f }; // RAM ring
  constexpr D2D1_RECT_F CpuLabel = { 290.0f, 92.0f, 330.0f, 103.0f }; // CPU label
  constexpr D2D1_RECT_F RamLabel = { 338.0f, 92.0f, 378.0f, 103.0f }; // RAM label
  constexpr D2D1_RECT_F GpuLabel = { 290.0f, 108.0f, 378.0f, 119.0f }; // GPU label + value
  constexpr D2D1_ROUNDED_RECT GpuBar = { { 290.0f, 123.0f, 378.0f, 127.0f }, 2.0f, 2.0f }; // GPU micro-pill
}
namespace ExpandedTelemetry {
  constexpr D2D1_ROUNDED_RECT TabTrack = { { 16.0f, 12.0f, 268.0f, 34.0f }, 11.0f, 11.0f }; // Tab track
  constexpr D2D1_ROUNDED_RECT TabSel = { { 102.0f, 14.0f, 182.0f, 32.0f }, 9.0f, 9.0f }; // Active tab pill
  constexpr D2D1_RECT_F Tab0 = { 16.0f, 12.0f, 100.0f, 34.0f }; // Tab · Now Playing
  constexpr D2D1_RECT_F Tab1 = { 100.0f, 12.0f, 184.0f, 34.0f }; // Tab · Telemetry
  constexpr D2D1_RECT_F Tab2 = { 184.0f, 12.0f, 268.0f, 34.0f }; // Tab · Drop Tray
  constexpr D2D1_RECT_F Clock = { 304.0f, 12.0f, 384.0f, 34.0f }; // Clock
  constexpr D2D1_RECT_F Row0Label = { 16.0f, 46.0f, 136.0f, 58.0f }; // CPU label
  constexpr D2D1_RECT_F Row0Value = { 324.0f, 46.0f, 384.0f, 58.0f }; // CPU value
  constexpr D2D1_ROUNDED_RECT Row0Bar = { { 16.0f, 61.0f, 384.0f, 65.0f }, 2.0f, 2.0f }; // CPU bar
  constexpr D2D1_RECT_F Row1Label = { 16.0f, 68.0f, 136.0f, 80.0f }; // RAM label
  constexpr D2D1_RECT_F Row1Value = { 324.0f, 68.0f, 384.0f, 80.0f }; // RAM value
  constexpr D2D1_ROUNDED_RECT Row1Bar = { { 16.0f, 83.0f, 384.0f, 87.0f }, 2.0f, 2.0f }; // RAM bar
  constexpr D2D1_RECT_F Row2Label = { 16.0f, 90.0f, 136.0f, 102.0f }; // GPU label
  constexpr D2D1_RECT_F Row2Value = { 324.0f, 90.0f, 384.0f, 102.0f }; // GPU value
  constexpr D2D1_ROUNDED_RECT Row2Bar = { { 16.0f, 105.0f, 384.0f, 109.0f }, 2.0f, 2.0f }; // GPU bar
  constexpr D2D1_RECT_F Row3Label = { 16.0f, 112.0f, 136.0f, 124.0f }; // NET label
  constexpr D2D1_RECT_F Row3Value = { 324.0f, 112.0f, 384.0f, 124.0f }; // NET value
  constexpr D2D1_ROUNDED_RECT Row3Bar = { { 16.0f, 127.0f, 384.0f, 131.0f }, 2.0f, 2.0f }; // NET bar
}
namespace ExpandedDropTray {
  constexpr D2D1_ROUNDED_RECT TabTrack = { { 16.0f, 12.0f, 268.0f, 34.0f }, 11.0f, 11.0f }; // Tab track
  constexpr D2D1_ROUNDED_RECT TabSel = { { 186.0f, 14.0f, 266.0f, 32.0f }, 9.0f, 9.0f }; // Active tab pill
  constexpr D2D1_RECT_F Tab0 = { 16.0f, 12.0f, 100.0f, 34.0f }; // Tab · Now Playing
  constexpr D2D1_RECT_F Tab1 = { 100.0f, 12.0f, 184.0f, 34.0f }; // Tab · Telemetry
  constexpr D2D1_RECT_F Tab2 = { 184.0f, 12.0f, 268.0f, 34.0f }; // Tab · Drop Tray
  constexpr D2D1_RECT_F Clock = { 304.0f, 12.0f, 384.0f, 34.0f }; // Clock
  constexpr D2D1_ROUNDED_RECT Drop = { { 16.0f, 46.0f, 384.0f, 134.0f }, 16.0f, 16.0f }; // Drop zone
  constexpr D2D1_ROUNDED_RECT Tile0 = { { 29.0f, 58.0f, 135.0f, 122.0f }, 10.0f, 10.0f }; // File tile 1
  constexpr D2D1_ROUNDED_RECT Tile1 = { { 147.0f, 58.0f, 253.0f, 122.0f }, 10.0f, 10.0f }; // File tile 2
  constexpr D2D1_ROUNDED_RECT Tile2 = { { 265.0f, 58.0f, 371.0f, 122.0f }, 10.0f, 10.0f }; // File tile 3
}
namespace HudLevel {
  constexpr D2D1_RECT_F Icon = { 14.0f, 12.0f, 34.0f, 32.0f }; // Icon
  constexpr D2D1_ROUNDED_RECT Track = { { 44.0f, 20.0f, 156.0f, 24.0f }, 2.0f, 2.0f }; // Level track
  constexpr D2D1_ROUNDED_RECT Fill = { { 44.0f, 20.0f, 156.0f, 24.0f }, 2.0f, 2.0f }; // Level fill
  constexpr D2D1_RECT_F Value = { 160.0f, 14.0f, 186.0f, 30.0f }; // Level value
}
namespace HudPomodoro {
  constexpr D2D1_ROUNDED_RECT Ring = { { 14.0f, 12.0f, 34.0f, 32.0f }, 10.0f, 10.0f }; // Timer ring
  constexpr D2D1_RECT_F Label = { 44.0f, 14.0f, 134.0f, 30.0f }; // Session label
  constexpr D2D1_RECT_F Time = { 136.0f, 14.0f, 186.0f, 30.0f }; // Time left
}

namespace Color { // straight (non-premultiplied) alpha
  constexpr D2D1_COLOR_F Transparent = { 0.000f, 0.000f, 0.000f, 0.000f }; // Swap-chain clear
  constexpr D2D1_COLOR_F Core = { 0.000f, 0.000f, 0.000f, 1.000f }; // #000000 @ 1
  constexpr D2D1_COLOR_F Obsidian = { 0.047f, 0.047f, 0.055f, 0.920f }; // #0C0C0E @ 0.92
  constexpr D2D1_COLOR_F Rim = { 1.000f, 1.000f, 1.000f, 0.120f }; // #FFFFFF @ 0.12
  constexpr D2D1_COLOR_F Aqua = { 0.000f, 1.000f, 1.000f, 1.000f }; // #00FFFF @ 1
  constexpr D2D1_COLOR_F Sky = { 0.220f, 0.741f, 0.973f, 1.000f }; // #38BDF8 @ 1
  constexpr D2D1_COLOR_F Glow = { 0.220f, 0.741f, 0.973f, 0.450f }; // #38BDF8 @ 0.45
  constexpr D2D1_COLOR_F Label = { 0.961f, 0.961f, 0.969f, 1.000f }; // #F5F5F7 @ 1
  constexpr D2D1_COLOR_F Label2 = { 0.922f, 0.922f, 0.961f, 0.600f }; // #EBEBF5 @ 0.6
  constexpr D2D1_COLOR_F Label3 = { 0.922f, 0.922f, 0.961f, 0.300f }; // #EBEBF5 @ 0.3
  constexpr D2D1_COLOR_F Fill = { 1.000f, 1.000f, 1.000f, 0.080f }; // #FFFFFF @ 0.08
  constexpr D2D1_COLOR_F Pill = { 1.000f, 1.000f, 1.000f, 0.160f }; // #FFFFFF @ 0.16
  constexpr D2D1_COLOR_F Track = { 1.000f, 1.000f, 1.000f, 0.140f }; // #FFFFFF @ 0.14
  constexpr D2D1_COLOR_F Lens = { 0.039f, 0.039f, 0.071f, 1.000f }; // #0A0A12 @ 1
  constexpr D2D1_COLOR_F LensRing = { 0.110f, 0.110f, 0.149f, 1.000f }; // #1C1C26 @ 1
  constexpr D2D1_COLOR_F Focus = { 1.000f, 0.624f, 0.039f, 1.000f }; // #FF9F0A @ 1
  constexpr D2D1_COLOR_F Shadow = { 0.000f, 0.000f, 0.000f, 0.550f }; // #000000 @ 0.55
}

// Restored M0/M1 tokens below (DESIGN Lampiran B); v0.2 values above win on conflict.
namespace Typography {
  enum class Role : std::size_t { TabClock, MediaTitle, Artist, Timestamp, TelemetryValue, MicroLabel, HudValue, Count };
  struct Style {
    const wchar_t* family;
    float size;
    DWRITE_FONT_WEIGHT weight;
    float lineHeight;
    float tracking;
    bool tabular;
    bool capitals;
    D2D1_COLOR_F color;
  };
  constexpr wchar_t FallbackFamily[] = L"Segoe UI";
  constexpr wchar_t Locale[] = L"en-US";
  constexpr Style TabClock = { L"Segoe UI Variable Text", 11.0f, DWRITE_FONT_WEIGHT_SEMI_BOLD, 14.0f, 0.1f, true, false, Color::Label };
  constexpr Style MediaTitle = { L"Georgia", 15.0f, DWRITE_FONT_WEIGHT_SEMI_BOLD, 18.0f, -0.2f, false, false, Color::Label };
  constexpr Style Artist = { L"Segoe UI Variable Text", 12.0f, DWRITE_FONT_WEIGHT_MEDIUM, 14.0f, 0.0f, false, false, Color::Label2 };
  constexpr Style Timestamp = { L"Segoe UI Variable Text", 10.0f, DWRITE_FONT_WEIGHT_MEDIUM, 11.0f, 0.0f, true, false, Color::Label3 };
  constexpr Style TelemetryValue = { L"Segoe UI Variable Display", 11.0f, DWRITE_FONT_WEIGHT_SEMI_BOLD, 12.0f, 0.0f, true, false, Color::Label };
  constexpr Style MicroLabel = { L"Segoe UI Variable Small", 9.5f, DWRITE_FONT_WEIGHT_SEMI_BOLD, 11.0f, 0.6f, false, true, Color::Label3 };
  constexpr Style HudValue = { L"Segoe UI Variable Display", 13.0f, DWRITE_FONT_WEIGHT_SEMI_BOLD, 16.0f, 0.0f, true, false, Color::Label };
  constexpr std::array<Style, static_cast<std::size_t>(Role::Count)> Styles = {
    TabClock, MediaTitle, Artist, Timestamp, TelemetryValue, MicroLabel, HudValue
  };
}
namespace DebugHud {
  constexpr D2D1_RECT_F FpsRect = { 8.0f, 6.0f, 64.0f, 17.0f }; // Body-local, 56 x 11 DIP.
  constexpr Typography::Role TextRole = Typography::Role::MicroLabel;
  constexpr bool Tabular = true;
  constexpr DWRITE_TEXT_ALIGNMENT Alignment = DWRITE_TEXT_ALIGNMENT_LEADING; // Left, LTR.
  constexpr float ActiveOpacity = 0.75f;
  constexpr D2D1_COLOR_F ActiveColor = { Color::Aqua.r, Color::Aqua.g, Color::Aqua.b, Color::Aqua.a * ActiveOpacity };
  constexpr D2D1_COLOR_F IdleColor = Color::Label3; // Alpha 0.30 exactly once.
  constexpr wchar_t IdleText[] = L"IDLE";
  constexpr bool ShowDebugFpsDefault = false; // Ctrl+Shift+F; independent of Ctrl+Alt+F12 bounds.
  constexpr bool ShowBoundsDefault = false;
}

namespace Motion {
  constexpr double FixedStep = 1.0 / 240.0;
  constexpr double MaxFrameDelta = 1.0 / 20.0;
  constexpr double MillisecondsPerSecond = 1000.0;
  // Compatibility aliases: Timing is the single source (v0.2 hover delay is 150, not 120).
  constexpr unsigned HoverEnterMs = Timing::HoverDelayMsDefault;
  constexpr unsigned HoverLeaveMs = Timing::CollapseDelayMs;
  constexpr unsigned HudMs = Timing::HudVisibleMs;
  constexpr unsigned FadeInMs = Timing::ContentFadeInMs;
  constexpr unsigned FadeDelayMs = Timing::ContentFadeDelayMs;
  constexpr unsigned FadeOutMs = Timing::ContentFadeOutMs; // Linear.
  constexpr unsigned ReducedFadeMs = 100; // Both fades, no delay/blur; fade-in cubic, fade-out linear.
  constexpr float ContentBlur = 3.0f;
  constexpr std::array<float, 4> FadeBezier = { 0.2f, 0.8f, 0.2f, 1.0f };
  constexpr std::array<unsigned, 4> WavePeriodsMs = { 820, 640, 1040, 760 };
  constexpr std::array<int, 4> WavePhaseMs = { -120, -380, -60, -520 }; // CSS animation-delay.
  constexpr float WaveMinimumScale = 0.28f; // 16 DIP * 0.28 = 4.48 DIP, user-approved.
  constexpr float ReducedMotionWaveHeight = 10.0f; // Static bars, DIP, centered on y = 17.
  // CSS Easing Functions Level 1: ease-in-out is this cubic, not a new design curve.
  constexpr std::array<float, 4> WaveBezier = { 0.42f, 0.0f, 0.58f, 1.0f };
  // Each half-cycle uses WaveBezier; negative delay starts partway into the cycle.
}
namespace PaintSpec {
  constexpr float RingStroke = 3.5f;
  constexpr float PomodoroStroke = 2.5f;
  constexpr float DropStroke = 1.0f;
  constexpr float DropDash = 4.0f;
  constexpr float DropGap = 3.0f;
}
namespace ExpandedNowPlaying {
  constexpr float TabStride = Tab1.left - Tab0.left;
}
namespace ExpandedDropTray {
  constexpr D2D1_POINT_2F TileIconOffset = { 12.0f, 12.0f };
  constexpr D2D1_POINT_2F TileNameOffset = { 12.0f, 40.0f };
  constexpr float TileNameRightInset = 10.0f;
}

namespace Glyph {
  // Exact HTML G SVG coordinates, local to the view box. Center media view boxes in buttons.
  constexpr D2D1_SIZE_F SkipSize = { 14.0f, 12.0f };
  constexpr D2D1_SIZE_F PlaySize = { 14.0f, 14.0f };
  constexpr D2D1_SIZE_F HudSize = { 20.0f, 20.0f };
  constexpr D2D1_SIZE_F FileSize = HudSize;
  // Filled closed polygons; separate figures for the two skip triangles.
  constexpr std::array<D2D1_POINT_2F, 3> PrevLeft = {{ { 7.0f, 1.0f }, { 1.0f, 6.0f }, { 7.0f, 11.0f } }};
  constexpr std::array<D2D1_POINT_2F, 3> PrevRight = {{ { 13.0f, 1.0f }, { 7.0f, 6.0f }, { 13.0f, 11.0f } }};
  constexpr std::array<D2D1_POINT_2F, 3> NextLeft = {{ { 1.0f, 1.0f }, { 7.0f, 6.0f }, { 1.0f, 11.0f } }};
  constexpr std::array<D2D1_POINT_2F, 3> NextRight = {{ { 7.0f, 1.0f }, { 13.0f, 6.0f }, { 7.0f, 11.0f } }};
  constexpr std::array<D2D1_POINT_2F, 3> Play = {{ { 3.0f, 1.5f }, { 3.0f, 12.5f }, { 12.5f, 7.0f } }};
  constexpr std::array<D2D1_ROUNDED_RECT, 2> Pause = {{
    { { 2.5f, 1.5f, 5.7f, 12.5f }, 1.0f, 1.0f },
    { { 8.3f, 1.5f, 11.5f, 12.5f }, 1.0f, 1.0f }
  }};
  constexpr std::array<D2D1_POINT_2F, 6> Speaker = {{
    { 3.0f, 7.5f }, { 6.0f, 7.5f }, { 10.5f, 3.5f },
    { 10.5f, 16.5f }, { 6.0f, 12.5f }, { 3.0f, 12.5f }
  }};
  struct Arc { D2D1_POINT_2F start; D2D1_ARC_SEGMENT segment; };
  constexpr std::array<Arc, 2> SpeakerArcs = {{
    { { 13.5f, 6.5f }, { { 13.5f, 13.5f }, { 5.0f, 5.0f }, 0.0f, D2D1_SWEEP_DIRECTION_CLOCKWISE, D2D1_ARC_SIZE_SMALL } },
    { { 15.8f, 4.2f }, { { 15.8f, 15.8f }, { 8.2f, 8.2f }, 0.0f, D2D1_SWEEP_DIRECTION_CLOCKWISE, D2D1_ARC_SIZE_SMALL } }
  }};
  constexpr float SpeakerStroke = 1.5f; // Open arcs, round caps, no fill.
  constexpr D2D1_ELLIPSE Sun = { { 10.0f, 10.0f }, 3.6f, 3.6f }; // Filled.
  constexpr std::array<D2D1_POINT_2F, 2> SunRay = {{ { 10.0f, 2.4f }, { 10.0f, 4.2f } }};
  constexpr std::array<float, 8> SunRayAngles = { 0.0f, 45.0f, 90.0f, 135.0f, 180.0f, 225.0f, 270.0f, 315.0f };
  constexpr float SunStroke = 1.6f; // Rotate ray about Sun.point, degrees; round caps.
  constexpr std::array<D2D1_POINT_2F, 5> FileOutline = {{
    { 5.0f, 2.5f }, { 11.5f, 2.5f }, { 15.5f, 6.5f }, { 15.5f, 17.5f }, { 5.0f, 17.5f }
  }}; // Closed, stroked, no fill.
  constexpr std::array<D2D1_POINT_2F, 3> FileFold = {{ { 11.5f, 2.5f }, { 11.5f, 6.5f }, { 15.5f, 6.5f } }};
  constexpr float FileStroke = 1.4f; // Open fold; both paths have round joins and flat caps.
}

namespace Demo {
  // Fictional HTML TRACKS and initial App state, not live system measurements.
  struct Track { const wchar_t* title; const wchar_t* artist; unsigned durationSeconds; };
  constexpr std::array<Track, 3> Tracks = {{
    { L"Glass Harbor", L"Mira Okonkwo", 214 },
    { L"Low Orbit Lullaby", L"The Quiet Engines", 188 },
    { L"Monsoon Static", L"Aru Prasetyo", 242 }
  }};
  constexpr unsigned InitialTrack = 0;
  constexpr unsigned ElapsedSeconds = 71;
  constexpr bool Playing = true;
  constexpr unsigned SecondsPerMinute = 60;
  constexpr float Percent = 100.0f;
  constexpr float Volume = 64.0f;
  constexpr float Brightness = 72.0f;
  constexpr std::array<float, 4> Telemetry = { 34.0f, 58.0f, 41.0f, 22.0f }; // CPU, RAM, GPU, NET percentages.
  constexpr float RamTotalGb = 32.0f;
  constexpr float RamGbPerPercent = 0.32f;
  constexpr float NetMbPerSecondPerPercent = 0.42f;
  constexpr unsigned PomodoroLeftSeconds = 18 * SecondsPerMinute + 42;
  constexpr unsigned PomodoroTotalSeconds = 25 * SecondsPerMinute;
  constexpr wchar_t PomodoroLabel[] = L"Focus";
  constexpr std::array<const wchar_t*, 3> TabLabels = { L"Now Playing", L"Telemetry", L"Drop Tray" };
  constexpr std::array<const wchar_t*, 4> TelemetryLabels = { L"CPU", L"RAM", L"GPU", L"NET" };
  constexpr std::array<const wchar_t*, 3> FileNames = { L"spec-sheet.pdf", L"notch@2x.png", L"build-0412.zip" };
  constexpr D2D1_COLOR_F ArtworkPlaceholder = Color::Label; // PRD R1; no procedural HTML artwork imported.
}

struct SpringParams { float stiffness, damping, mass; };
namespace Spring {
  constexpr SpringParams IdleToCompact { 223.8f, 23.94f, 1.0f }; // response 0.42s, damping ratio 0.8
  constexpr SpringParams ToExpanded { 171.3f, 19.37f, 1.0f }; // response 0.48s, damping ratio 0.74
  constexpr SpringParams Collapse { 304.6f, 32.11f, 1.0f }; // response 0.36s, damping ratio 0.92
  constexpr SpringParams CompactToIdle { 341.5f, 35.11f, 1.0f }; // response 0.34s, damping ratio 0.95
  constexpr SpringParams ToHud { 438.6f, 28.48f, 1.0f }; // response 0.3s, damping ratio 0.68
  constexpr SpringParams FromHud { 273.4f, 29.10f, 1.0f }; // response 0.38s, damping ratio 0.88
  constexpr SpringParams HoverSwell { 503.6f, 27.83f, 1.0f }; // response 0.28s, damping ratio 0.62
}

// Defaults for %APPDATA%\NotchIsland\config.json. Used per field when the file,
// the field, or its value is missing or invalid.
enum class TitleFont { Serif, Sans };
struct UserConfig {
  int          version;
  TitleFont    titleFont;
  D2D1_COLOR_F accentColor;   // replaces Sky; see AccentStart()
  unsigned     hoverDelayMs;  // clamped to [0, 1000]
  int          monitorIndex;  // 0 = primary; 1..n = others sorted by left edge
};
constexpr UserConfig kDefaultConfig { 1, TitleFont::Serif, Color::Sky, 150u, 0 };

// Gradient start for the accent. The default accent keeps the designed Aqua -> Sky pair;
// a custom accent mixes 35% white into itself.
inline D2D1_COLOR_F AccentStart(const D2D1_COLOR_F& accent) {
  const bool isDefault = std::fabs(accent.r - Color::Sky.r) < 0.002f &&
                         std::fabs(accent.g - Color::Sky.g) < 0.002f &&
                         std::fabs(accent.b - Color::Sky.b) < 0.002f;
  if (isDefault) return Color::Aqua;
  return { accent.r + (1.0f - accent.r) * 0.35f, accent.g + (1.0f - accent.g) * 0.35f,
           accent.b + (1.0f - accent.b) * 0.35f, 1.0f };
}

struct SpringValue { float x, v, target; };
// Semi-implicit Euler. Run at a fixed dt = 1/240 s, accumulating real frame time.
inline void StepSpring(SpringValue& s, const SpringParams& p, float dt) {
  const float a = (-p.stiffness * (s.x - s.target) - p.damping * s.v) / p.mass;
  s.v += a * dt;
  s.x += s.v * dt;
}
inline bool IsSettled(const SpringValue& s) {
  return std::fabs(s.x - s.target) < 0.01f && std::fabs(s.v) < 0.01f;
}

// Outline with concave "liquid ears" and continuous-curvature bottom corners.
// Rebuild every animation frame from the sprung width/height/radii.
inline HRESULT BuildIslandGeometry(ID2D1Factory* factory, float W, float H, float rb, float e,
                                   ID2D1PathGeometry** out) {
  e  = (std::max)(0.0f, e);
  const float c = (std::max)(0.0f, (std::min)({ rb * 1.25f, W * 0.5f, H - e })); // corner extent
  const float k = 0.30f * c;                                                    // handle offset from corner
  ID2D1PathGeometry* geo = nullptr;
  HRESULT hr = factory->CreatePathGeometry(&geo);
  if (FAILED(hr)) return hr;
  ID2D1GeometrySink* s = nullptr;
  hr = geo->Open(&s);
  if (FAILED(hr)) { geo->Release(); return hr; }
  s->BeginFigure(D2D1::Point2F(-e, 0), D2D1_FIGURE_BEGIN_FILLED);
  s->AddLine(D2D1::Point2F(W + e, 0));
  if (e > 0) s->AddArc(D2D1::ArcSegment(D2D1::Point2F(W, e), D2D1::SizeF(e, e), 0.0f,
                       D2D1_SWEEP_DIRECTION_COUNTER_CLOCKWISE, D2D1_ARC_SIZE_SMALL));
  s->AddLine(D2D1::Point2F(W, H - c));
  s->AddBezier(D2D1::BezierSegment(D2D1::Point2F(W, H - k), D2D1::Point2F(W - k, H), D2D1::Point2F(W - c, H)));
  s->AddLine(D2D1::Point2F(c, H));
  s->AddBezier(D2D1::BezierSegment(D2D1::Point2F(k, H), D2D1::Point2F(0, H - k), D2D1::Point2F(0, H - c)));
  s->AddLine(D2D1::Point2F(0, e));
  if (e > 0) s->AddArc(D2D1::ArcSegment(D2D1::Point2F(-e, 0), D2D1::SizeF(e, e), 0.0f,
                       D2D1_SWEEP_DIRECTION_COUNTER_CLOCKWISE, D2D1_ARC_SIZE_SMALL));
  s->EndFigure(D2D1_FIGURE_END_CLOSED);
  hr = s->Close();
  s->Release();
  if (FAILED(hr)) { geo->Release(); return hr; }
  *out = geo;
  return S_OK;
}

// Surface color for a sprung mix value m in [0, 1]:
//   rgb = lerp(Core.rgb, Obsidian.rgb, m), a = lerp(1.0, 0.92, m)
// Rim: linear gradient brush y 0 -> H, alpha 0.12 -> 0.04, multiplied by geom.rim.
// Draw it as a 2-DIP stroke inside PushLayer(geometric mask = island) so only 1 DIP shows.

} // namespace NotchIsland
```

---

## Lampiran B — Kompatibilitas M0/M1 (dipulihkan di v0.2)

Penggantian utuh ke v0.2 sempat menghapus token M0/M1 yang sudah dipakai kode. SPEC-PATCH v0.2 menyatakan header v0.2 "hanya menambah konstanta", jadi token tersebut dipulihkan dari versi sebelumnya (git `HEAD`) ke Lampiran A tanpa mengubah nilai v0.2. Jika ada konflik, nilai v0.2 menang dan token lama menjadi alias.

| Namespace / token | Status | Catatan |
|---|---|---|
| `WindowSpec`, `PaintSpec` (M0) | Dipulihkan apa adanya | Window tetap 492 × 193 DIP, anchor ke `rcMonitor` (bukan `rcWork`). Karena semua lebar geom genap, body di tengah window = rumus anchor Bagian 2, jadi keduanya setara. Stroke tetap dalam DIP: lens/debug 1, rim 2 (di-mask jadi 1 di dalam). |
| `Idle::Body/LeftEar/RightEar`, `Color::Transparent` | Dipulihkan | Rect debug turunan `kGeomIdle`; clear swap chain `{0, 0, 0, 0}`. |
| `Typography` | Dipulihkan | Tabel Bagian 7 sebagai data. `MediaTitle` = default `titleFont` serif; varian sans memakai family Bagian 7 (implementasi config di luar lingkup ini). `FallbackFamily` hanya untuk kegagalan DirectWrite, bukan fallback Windows 10. |
| `DebugHud` | Dipulihkan tanpa redesain | `FpsRect = {8, 6, 64, 17}`, micro label tabular, aktif Aqua × 0.75, `IDLE` dengan `Label3` (alpha 0.30 sekali). Dua toggle **independen**, default mati: FPS `Ctrl+Shift+F`, bounds `Ctrl+Alt+F12`. Tidak ada timer render hanya untuk FPS. |
| `Motion` | Dipulihkan; timing jadi alias | `HoverEnterMs = Timing::HoverDelayMsDefault` (150, **bukan** 120 lama), `HoverLeaveMs = Timing::CollapseDelayMs` (250), `HudMs`, `FadeInMs`, `FadeDelayMs`, `FadeOutMs` → `Timing::*`. Nilai baru: `ReducedMotionWaveHeight = 10` DIP (Bagian 8.3). Fixed step 1/240, maks delta 1/20, fade bezier, periode/fase wave, `WaveBezier` (CSS `ease-in-out`) tetap. |
| Waveform minimum | Dipertahankan | Keputusan pengguna M1: `WaveMinimumScale = 0.28`, `16 × 0.28 = 4.48` DIP. Prosa v0.2 yang menyebut 4 atau 4.5 diselaraskan ke **4.48**. Paused tetap titik 3 DIP (`kPausedDotHeight`); reduced motion statis 10 DIP. |
| `PaintSpec` ring/drop, `TabStride`, offset tile | Dipulihkan | Stroke ring 3.5/2.5, dash drop 1/4/3, stride tab 84, offset icon/nama tile (12,12)/(12,40), inset kanan 10. |
| `Glyph`, `Demo` | Dipulihkan apa adanya | Terjemahan SVG `G` HTML dan sample deterministik HTML (bukan data sistem). Artwork placeholder `Color::Label`. |

- Reduced motion fade: masuk/keluar masing-masing 100 ms tanpa delay/blur; masuk cubic `(0.2, 0.8, 0.2, 1)`, keluar linear.
- `tools/verify-spec.cjs` memeriksa: header = Lampiran A, nilai HTML asli untuk token yang berasal dari HTML, tambahan v0.2 (`Dot0–3`, swell, paused, `Timing`, `HoverSwell`, `kDefaultConfig`) terhadap tabel DESIGN, serta keberadaan token Lampiran B ini dan alias `Motion` → `Timing`.
- Perbedaan referensi HTML yang tetap berlaku: tab bergeser dengan pegas `ToExpanded` (bukan CSS 380 ms); pause membekukan bar jadi titik 3 DIP (bukan fase HTML); hover memakai 150/250 ms (HTML tanpa delay); telemetri 1000 ms dan 500 ms ease-out (HTML 1400/600 ms); dekorasi HTML tambahan (highlight, knob shadow, variasi glow) tidak diimpor.

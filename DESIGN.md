# DESIGN.md — Notch Island

Design system dan spesifikasi visual untuk Notch Island: overlay Windows ala macOS Notch / Dynamic Island yang menempel di tepi atas layar. Dokumen ini adalah **sumber kebenaran tunggal** untuk semua angka. Kode tidak boleh punya angka visual yang tidak tercantum di sini.

Prototipe interaktif (referensi visual): artifact **Notch Island** di claude.ai. Semua tabel di bawah dihasilkan dari spec yang sama dengan prototipe.

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
| 2 | **Compact media** | Ada media yang sedang diputar (SMTC session aktif & playing). |
| 3 | **Expanded dashboard** | Kursor hover di atas island ≥ 120 ms, atau klik. Punya 3 tab: Now Playing, Telemetry, Drop Tray. |
| 4 | **System HUD** | Volume / brightness berubah, atau timer Pomodoro selesai. Tampil 1800 ms lalu kembali ke state sebelumnya. |

Prioritas jika bertabrakan: **HUD > Expanded (hover) > Compact > Dormant**. HUD yang muncul saat Expanded akan menggantikan Expanded, lalu kembali ke Expanded jika kursor masih di atas island.

### 3.1 Geometri per state

| State | W | H | Bottom r | Ear r | Body `D2D1_RECT_F` | Screen rect @1920 | Surface | Rim | Shadow |
|---|--:|--:|--:|--:|---|---|---|--:|--:|
| 1 · Dormant | 160 | 28 | 12 | 8 | `{0, 0, 160, 28}` | `{880, 0, 1040, 28}` | Core | 0 | 0 |
| 2 · Compact media | 240 | 34 | 17 | 9 | `{0, 0, 240, 34}` | `{840, 0, 1080, 34}` | Core | 0.5 | 0.25 |
| 3 · Expanded dashboard | 400 | 145 | 28 | 14 | `{0, 0, 400, 145}` | `{760, 0, 1160, 145}` | Obsidian | 1 | 1 |
| 4 · System HUD | 200 | 44 | 22 | 10 | `{0, 0, 200, 44}` | `{860, 0, 1060, 44}` | Core | 0.75 | 0.4 |

- **Rim** dan **Shadow** adalah pengali 0–1 untuk token `Specular rim` dan `Drop shadow`.
- **Surface mix**: 0 = Core, 1 = Obsidian. Hanya Expanded yang bernilai 1.
- Ukuran window overlay tetap: **492 × 193 DIP** (envelope terbesar 428 × 145 + ruang shadow), di-anchor di tengah atas. Bentuk dianimasikan di dalam window; window **tidak** di-resize per frame.

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
| Wave bar 1 | 209 | 9 | 3 | 16 | 1.5 | `{209, 9, 212, 25}` | Sky | Height 4–16, centered on y = 17. Aqua → Sky gradient |
| Wave bar 2 | 215 | 9 | 3 | 16 | 1.5 | `{215, 9, 218, 25}` | Sky | Height 4–16, centered on y = 17. Aqua → Sky gradient |
| Wave bar 3 | 221 | 9 | 3 | 16 | 1.5 | `{221, 9, 224, 25}` | Sky | Height 4–16, centered on y = 17. Aqua → Sky gradient |
| Wave bar 4 | 227 | 9 | 3 | 16 | 1.5 | `{227, 9, 230, 25}` | Sky | Height 4–16, centered on y = 17. Aqua → Sky gradient |

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
| Wave bar i | `x = 209 + 6i`, `w = 3`, `h ∈ [4, 16]`, pusat vertikal `y = 17` |
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
- Kontras: semua teks Label primary/secondary di atas Core/Obsidian lolos WCAG AA. Label tertiary hanya untuk label mikro dan timestamp, bukan informasi penting.

---

## 7. Tipografi

| Role | macOS | Windows (DirectWrite) | Size | Weight | Line | Tracking | Color |
|---|---|---|--:|--:|--:|---|---|
| Tab label / clock | SF Pro Text | Segoe UI Variable Text | 11 | 600 | 14 | +0.1 | Label primary |
| Media title | New York | Georgia | 15 | 600 | 18 | -0.2 | Label primary |
| Artist | SF Pro Text | Segoe UI Variable Text | 12 | 500 | 14 | 0 | Label secondary |
| Timestamp | SF Pro Text | Segoe UI Variable Text | 10 | 500 | 11 | 0 · tabular | Label tertiary |
| Telemetry value | SF Pro Display | Segoe UI Variable Display | 11 | 600 | 12 | 0 · tabular | Label primary |
| Micro label | SF Pro Text | Segoe UI Variable Small | 9.5 | 600 | 11 | +0.6 · caps | Label tertiary |
| HUD value | SF Pro Display | Segoe UI Variable Display | 13 | 600 | 16 | 0 · tabular | Label primary |

- **Jangan bundle SF Pro atau New York.** Lisensinya hanya untuk platform Apple. Default Windows: **Segoe UI Variable** (bawaan Windows 11). Fallback Windows 10: Segoe UI. Opsional: bundle **Inter** (OFL) dan muat lewat `IDWriteFactory5::CreateInMemoryFontFileLoader`.
- Serif aksen (judul lagu): **Georgia**.
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

- Rumus: `k = (2π / response)² × m`, `c = 4π × ζ × m / response`, `m = 1`.
- **Settle** = waktu terakhir nilai menyimpang > 0.5% dari target.
- Integrator: **semi-implicit Euler, fixed step 1/240 s**, akumulasikan waktu frame nyata (maksimum 1/20 s per frame agar tidak meledak setelah sleep).
- Nilai yang dipegas bersama: `width, height, bottomRadius, earRadius, surfaceMix, rim, shadow`.
- Transisi yang terputus di tengah (mis. hover lalu cepat keluar) **meneruskan velocity** saat ini ke target baru. Jangan reset ke 0.
- Animasi berhenti saat semua nilai `|x − target| < 0.01` dan `|v| < 0.01`. Setelah itu stop render loop (hemat CPU/GPU).

### 8.2 Timing lain

| Hal | Nilai |
|---|---|
| Delay hover → Expanded | 120 ms |
| Delay keluar → collapse | 250 ms (mencegah flicker saat kursor menyerempet tepi) |
| HUD tampil | 1800 ms sejak event terakhir. Event baru me-reset timer. |
| Fade-in konten | 240 ms, delay 70 ms, cubic-bezier(0.2, 0.8, 0.2, 1), opacity 0→1 + blur 3→0 DIP |
| Fade-out konten | 120 ms linear, mulai bersamaan dengan pegas |
| Wave bar | durasi siklus 820 / 640 / 1040 / 760 ms, scaleY 0.28 ↔ 1.0, ease-in-out. Berhenti di 0.28 saat pause. |
| Tekan tombol media | scale 0.88 + fill `Selected pill`, 120 ms |
| Update telemetri | tiap 1000 ms, perubahan nilai dianimasikan 500 ms ease-out |

### 8.3 Reduced motion

Jika `SPI_GETCLIENTAREAANIMATION` = FALSE: lompati pegas (snap ke target), matikan wave bar animasi (tampilkan bar statis), fade jadi 100 ms tanpa blur.

---

## 9. Interaksi

| Input | Perilaku |
|---|---|
| Hover di island (Dormant/Compact) | Setelah 120 ms → Expanded, tab terakhir dipakai. |
| Kursor keluar dari Expanded | Setelah 250 ms → kembali ke state dasar. |
| Klik tab | Pindah tab, pill bergeser. |
| Klik Prev / Play-Pause / Next | Kirim ke SMTC session aktif. |
| Drag di scrub | Pointer capture, seek saat dilepas (preview posisi saat drag). |
| Drag file ke island | Expanded langsung terbuka di tab Drop Tray. |
| Klik di luar bentuk island | Tembus ke aplikasi di bawah (`HTTRANSPARENT`). |
| Aplikasi fullscreen di foreground | Island disembunyikan (kecuali HUD). |

Area hit-test = geometri island saat ini (`ID2D1Geometry::FillContainsPoint`), bukan bounding box window.

---

## 10. Checklist verifikasi visual

- [ ] Pada 100% dan 150% scaling, screenshot island cocok dengan prototipe (overlay di atas screenshot prototipe zoom 1×/1.5×, selisih ≤ 1 px).
- [ ] Debug overlay (toggle `Ctrl+Alt+F12`) menggambar semua rect di Bagian 5 dengan stroke 1 px Aqua, dan semua elemen jatuh tepat di dalamnya.
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
#include <algorithm>
#include <cmath>

namespace NotchIsland {

struct IslandGeom { float width, height, bottomRadius, earRadius, surfaceMix, rim, shadow; };
constexpr IslandGeom kGeomIdle { 160.0f, 28.0f, 12.0f, 8.0f, 0.0f, 0.0f, 0.0f };
constexpr IslandGeom kGeomCompact { 240.0f, 34.0f, 17.0f, 9.0f, 0.0f, 0.5f, 0.25f };
constexpr IslandGeom kGeomExpanded { 400.0f, 145.0f, 28.0f, 14.0f, 1.0f, 1.0f, 1.0f };
constexpr IslandGeom kGeomHud { 200.0f, 44.0f, 22.0f, 10.0f, 0.0f, 0.75f, 0.4f };

// Screen anchor: left = floor((monitorWidthDip - geom.width) / 2), top = monitor work-area top (0).

namespace Idle {
  constexpr D2D1_ROUNDED_RECT Lens = { { 76.0f, 10.0f, 84.0f, 18.0f }, 4.0f, 4.0f }; // Camera lens
  constexpr D2D1_ROUNDED_RECT Status = { { 138.0f, 11.0f, 144.0f, 17.0f }, 3.0f, 3.0f }; // Status dot
}
namespace Compact {
  constexpr D2D1_ROUNDED_RECT Art = { { 10.0f, 6.0f, 32.0f, 28.0f }, 6.0f, 6.0f }; // Album art
  constexpr D2D1_RECT_F Wave = { 209.0f, 9.0f, 230.0f, 25.0f }; // Waveform group
  constexpr D2D1_ROUNDED_RECT Bar0 = { { 209.0f, 9.0f, 212.0f, 25.0f }, 1.5f, 1.5f }; // Wave bar 1
  constexpr D2D1_ROUNDED_RECT Bar1 = { { 215.0f, 9.0f, 218.0f, 25.0f }, 1.5f, 1.5f }; // Wave bar 2
  constexpr D2D1_ROUNDED_RECT Bar2 = { { 221.0f, 9.0f, 224.0f, 25.0f }, 1.5f, 1.5f }; // Wave bar 3
  constexpr D2D1_ROUNDED_RECT Bar3 = { { 227.0f, 9.0f, 230.0f, 25.0f }, 1.5f, 1.5f }; // Wave bar 4
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

struct SpringParams { float stiffness, damping, mass; };
namespace Spring {
  constexpr SpringParams IdleToCompact { 223.8f, 23.94f, 1.0f }; // response 0.42s, damping ratio 0.8
  constexpr SpringParams ToExpanded { 171.3f, 19.37f, 1.0f }; // response 0.48s, damping ratio 0.74
  constexpr SpringParams Collapse { 304.6f, 32.11f, 1.0f }; // response 0.36s, damping ratio 0.92
  constexpr SpringParams CompactToIdle { 341.5f, 35.11f, 1.0f }; // response 0.34s, damping ratio 0.95
  constexpr SpringParams ToHud { 438.6f, 28.48f, 1.0f }; // response 0.3s, damping ratio 0.68
  constexpr SpringParams FromHud { 273.4f, 29.10f, 1.0f }; // response 0.38s, damping ratio 0.88
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

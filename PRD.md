# PRD.md — Caecilia Notch Island

| | |
|---|---|
| Produk | Caecilia Notch Island — overlay Dynamic Island untuk Windows |
| Pemilik | Chris |
| Status | Draft v0.1 |
| Tanggal | 18 Oktober 2026 |
| Dokumen terkait | `DESIGN.md` (spesifikasi visual & angka), `NotchIslandSpec.h` |

---

## 1. Ringkasan

Caecilia Notch Island adalah aplikasi desktop Windows yang menampilkan "pulau" hitam kecil di tengah atas layar, meniru macOS Notch / Dynamic Island. Dalam keadaan diam ia hampir tak terlihat. Saat ada musik diputar ia menampilkan cover dan waveform kecil. Saat di-hover ia membesar jadi dashboard berisi kontrol media, telemetri sistem, dan tempat menaruh file sementara. Saat volume, brightness, atau timer berubah, ia menampilkan HUD singkat menggantikan OSD bawaan Windows.

Aplikasi ditulis dalam **C++ native dengan Direct2D**, agar ringan, mulus di 60–144 Hz, dan tidak bergantung pada runtime besar seperti Electron atau WebView.

## 2. Masalah

- Informasi kecil yang sering dilihat (lagu yang diputar, beban CPU/RAM, level volume) tersebar di taskbar, tray, dan OSD Windows yang posisinya tidak konsisten.
- OSD volume/brightness bawaan Windows menutupi konten di kiri atas atau bawah layar dan tidak bisa dikustomisasi.
- Alternatif yang ada umumnya dibangun di atas web runtime, makan 150–300 MB RAM, dan animasinya tidak semulus aslinya.

## 3. Tujuan

| ID | Tujuan | Ukuran keberhasilan |
|---|---|---|
| G1 | Satu tempat konsisten di atas layar untuk status media, sistem, dan HUD | Semua 4 state di `DESIGN.md` berfungsi |
| G2 | Terasa native dan halus | Animasi stabil di refresh rate monitor, tanpa frame drop terlihat |
| G3 | Ringan | Lihat target non-fungsional (Bagian 7) |
| G4 | Tidak mengganggu | Tidak pernah mengambil fokus, klik di luar bentuk tembus |

## 4. Bukan tujuan (v1)

- Dukungan multi-monitor (v1 hanya monitor utama).
- Notifikasi aplikasi pihak ketiga (WhatsApp, Discord, dll.).
- Tema terang atau kustomisasi warna oleh pengguna.
- Plugin / API eksternal.
- Dukungan Windows 10 di bawah build 19041 dan semua versi Windows sebelum 10.
- Menyembunyikan OSD bawaan Windows secara paksa (lihat Risiko R3).

## 5. Pengguna

- **Pengguna utama:** pengguna Windows 10/11 yang sering memutar musik sambil bekerja dan suka estetika macOS / minimalis.
- **Pengguna sekunder:** developer dan power user yang ingin melihat beban CPU/RAM/GPU sekilas tanpa membuka Task Manager.

### User stories

1. Sebagai pendengar musik, saya ingin melihat lagu yang sedang diputar dan menekan pause tanpa berpindah jendela.
2. Sebagai developer, saya ingin melihat CPU dan RAM sekilas saat build berjalan.
3. Sebagai pengguna laptop, saya ingin melihat level volume dan brightness di tempat yang sama setiap kali menekan tombol fungsi.
4. Sebagai pengguna Pomodoro, saya ingin tahu sesi fokus selesai tanpa suara notifikasi yang mengganggu.
5. Sebagai pengguna yang sering memindahkan file, saya ingin menaruh file sementara di island lalu menyeretnya keluar ke folder lain.

## 6. Kebutuhan fungsional

Prioritas: **P0** wajib untuk v1, **P1** sebaiknya ada di v1, **P2** nanti.

### 6.1 Shell & window

| ID | Kebutuhan | Prioritas |
|---|---|---|
| FR-01 | Window overlay borderless ukuran tetap 492 × 193 DIP, di-anchor di tengah atas monitor utama, selalu di atas (`WS_EX_TOPMOST`), tanpa ikon taskbar (`WS_EX_TOOLWINDOW`), tidak mengambil fokus (`WS_EX_NOACTIVATE`). | P0 |
| FR-02 | Render per-pixel alpha lewat DirectComposition + swap chain `DXGI_ALPHA_MODE_PREMULTIPLIED`. | P0 |
| FR-03 | Hit-test mengikuti bentuk island; area lain mengembalikan `HTTRANSPARENT`. | P0 |
| FR-04 | Mendukung per-monitor DPI v2; posisi dan ukuran benar pada 100–200% scaling, dan saat scaling berubah tanpa restart. | P0 |
| FR-05 | Menyembunyikan island saat aplikasi fullscreen aktif (`SHQueryUserNotificationState`), kecuali HUD. | P1 |
| FR-06 | Ikon tray dengan menu: Pause island, Mulai saat login, Keluar. | P1 |
| FR-07 | Opsi "Mulai saat login" lewat registry `HKCU\...\Run`. | P1 |
| FR-08 | Satu instance saja (named mutex). | P0 |

### 6.2 State & animasi

| ID | Kebutuhan | Prioritas |
|---|---|---|
| FR-10 | Empat state (Dormant, Compact, Expanded, HUD) dengan geometri persis seperti `DESIGN.md` Bagian 3. | P0 |
| FR-11 | Outline dengan liquid ears dan sudut bawah continuous curvature sesuai `DESIGN.md` Bagian 4. | P0 |
| FR-12 | Transisi antar state memakai pegas dari `DESIGN.md` Bagian 8, termasuk meneruskan velocity saat transisi terputus. | P0 |
| FR-13 | State machine dengan prioritas HUD > Expanded > Compact > Dormant. | P0 |
| FR-14 | Hover 120 ms membuka Expanded; keluar 250 ms menutupnya. | P0 |
| FR-15 | Reduced motion mengikuti setting Windows. | P1 |

### 6.3 Media (Compact & tab Now Playing)

| ID | Kebutuhan | Prioritas |
|---|---|---|
| FR-20 | Membaca sesi media aktif lewat `GlobalSystemMediaTransportControlsSessionManager` (C++/WinRT): judul, artis, thumbnail, status play, posisi, durasi. | P0 |
| FR-21 | Compact muncul otomatis saat ada sesi berstatus Playing; kembali ke Dormant 5 detik setelah pause/stop. | P0 |
| FR-22 | Tombol Prev, Play/Pause, Next mengirim perintah ke sesi aktif. | P0 |
| FR-23 | Scrub bar bisa di-drag untuk seek (`TryChangePlaybackPositionAsync`). Jika aplikasi tidak mendukung seek, bar tetap tampil tetapi tidak bisa di-drag. | P1 |
| FR-24 | Waveform 4 bar beranimasi saat playing (animasi dekoratif, bukan analisis audio sungguhan). | P0 |
| FR-25 | Waveform reaktif terhadap level audio nyata via `IAudioMeterInformation`. | P2 |

### 6.4 Telemetri

| ID | Kebutuhan | Prioritas |
|---|---|---|
| FR-30 | CPU % total via `GetSystemTimes`, update tiap 1 detik. | P0 |
| FR-31 | RAM % via `GlobalMemoryStatusEx`. | P0 |
| FR-32 | GPU % via PDH counter `\GPU Engine(*engtype_3D)\Utilization Percentage` (jumlah semua instance, clamp 100). | P1 |
| FR-33 | Network throughput via `GetIfTable2` (selisih byte per detik, semua adapter aktif). | P1 |
| FR-34 | Polling telemetri hanya berjalan saat Expanded terbuka di tab yang menampilkannya. | P0 |

### 6.5 HUD

| ID | Kebutuhan | Prioritas |
|---|---|---|
| FR-40 | Mendeteksi perubahan volume master lewat `IAudioEndpointVolumeCallback` dan menampilkan HUD level. | P0 |
| FR-41 | Mendeteksi perubahan brightness lewat WMI `WmiMonitorBrightnessEvent` (hanya layar internal laptop). | P1 |
| FR-42 | Timer Pomodoro (25 menit default) yang dimulai dari menu tray; HUD Pomodoro muncul saat sesi selesai dan saat sisa 1 menit. | P1 |
| FR-43 | HUD tampil 1800 ms; event baru me-reset timer. | P0 |

### 6.6 Drop Tray

| ID | Kebutuhan | Prioritas |
|---|---|---|
| FR-50 | Island menerima drag file (`IDropTarget`); saat file di-drag di atasnya, Expanded terbuka di tab Drop Tray. | P1 |
| FR-51 | Menyimpan referensi (path) maksimal 3 file, bukan menyalin file. | P1 |
| FR-52 | File di tray bisa di-drag keluar ke Explorer (`IDataObject` + `DoDragDrop`). | P1 |
| FR-53 | Tray dikosongkan saat aplikasi ditutup. | P1 |

### 6.7 Debug

| ID | Kebutuhan | Prioritas |
|---|---|---|
| FR-60 | `Ctrl+Alt+F12` menampilkan overlay bounding box semua elemen (stroke 1 px Aqua) dan FPS. | P0 |
| FR-61 | Mode demo (`--demo`) yang memutar data palsu untuk semua state tanpa butuh media/telemetri nyata. | P0 |

## 7. Kebutuhan non-fungsional

| ID | Kebutuhan | Target |
|---|---|---|
| NFR-01 | CPU saat diam (Dormant/Compact, tanpa animasi berjalan) | < 0.5% rata-rata di CPU 4-core modern |
| NFR-02 | CPU saat animasi state | < 5% |
| NFR-03 | Memori (working set) | < 40 MB |
| NFR-04 | Waktu mulai sampai island tampil | < 300 ms |
| NFR-05 | Frame rate animasi | Mengikuti refresh monitor (60–165 Hz), render loop berhenti saat tidak ada animasi |
| NFR-06 | Latensi hover → mulai animasi | ≤ 120 ms (sesuai delay desain) + 1 frame |
| NFR-07 | Ukuran installer/zip | < 5 MB |
| NFR-08 | Ketepatan visual | Selisih ≤ 1 px terhadap prototipe pada 100% dan 150% scaling |
| NFR-09 | Stabilitas | Bertahan saat GPU device lost (`D2DERR_RECREATE_TARGET`), sleep/resume, dan perubahan resolusi |
| NFR-10 | Privasi | Tidak ada koneksi jaringan keluar. Tidak ada telemetri pengguna. |

## 8. Teknologi

| Lapisan | Pilihan |
|---|---|
| Bahasa | C++20, MSVC (Visual Studio 2022) |
| Build | CMake ≥ 3.25 |
| Rendering | Direct2D 1.1 (`ID2D1DeviceContext`), Direct3D 11 device, DXGI swap chain flip-model |
| Komposisi | DirectComposition |
| Teks | DirectWrite |
| Media | C++/WinRT, `Windows.Media.Control` |
| Audio | Core Audio (`IMMDeviceEnumerator`, `IAudioEndpointVolume`) |
| Telemetri | Win32 API + PDH |
| Target OS | Windows 10 build 19041+ dan Windows 11 |

## 9. Rilis bertahap

| Milestone | Isi | Selesai jika |
|---|---|---|
| **M0 — Fondasi** | Window overlay, DComp + D2D, DPI, hit-test bentuk, debug overlay, mode demo | Island Dormant tampil tepat di tengah atas, klik di luar tembus |
| **M1 — Bentuk & gerak** | Outline, permukaan/rim/shadow, state machine, pegas, hover | Keempat state bisa dipicu di mode demo, cocok dengan prototipe |
| **M2 — Media** | SMTC, Compact, tab Now Playing, kontrol, scrub | Spotify / browser bisa dikontrol dari island |
| **M3 — Sistem** | Telemetri, HUD volume/brightness, Pomodoro, tray menu, autostart | Semua P0 + P1 selesai kecuali Drop Tray |
| **M4 — Drop Tray & polish** | Drag-drop, fullscreen hide, reduced motion, device-lost, installer | Semua kriteria Bagian 10 lolos |

## 10. Kriteria penerimaan v1

1. Semua kebutuhan P0 dan P1 terpenuhi.
2. Checklist verifikasi visual di `DESIGN.md` Bagian 10 lolos.
3. Semua target NFR-01 s.d. NFR-09 terukur dan lolos (catat hasil ukur di `docs/perf.md`).
4. Berjalan 8 jam tanpa crash, tanpa kebocoran memori (working set naik < 5 MB).
5. Diuji dengan: Spotify, YouTube di Chrome & Edge, Windows Media Player.

## 11. Risiko

| ID | Risiko | Mitigasi |
|---|---|---|
| R1 | Tidak semua aplikasi media mengisi SMTC dengan lengkap (thumbnail/posisi kosong) | Tampilkan artwork placeholder; sembunyikan scrub jika posisi tidak tersedia |
| R2 | Counter GPU PDH tidak tersedia di beberapa driver | Sembunyikan baris GPU jika counter gagal dibuka |
| R3 | OSD bawaan Windows tetap muncul berdampingan dengan HUD | v1 menerima ini. Menyembunyikan OSD bawaan butuh trik tidak resmi, ditunda |
| R4 | Window topmost tertutup aplikasi topmost lain atau game | Re-assert `HWND_TOPMOST` saat foreground berubah (`EVENT_SYSTEM_FOREGROUND`) |
| R5 | Monitor dengan notch kamera / bezel tipis membuat island menutupi konten | Opsi offset Y di config, default 0 |
| R6 | Shadow blur mahal di setiap frame | Render shadow ke bitmap cache saat diam; saat animasi pakai effect dengan kualitas `SPEED` |



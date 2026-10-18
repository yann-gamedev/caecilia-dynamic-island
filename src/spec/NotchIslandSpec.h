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

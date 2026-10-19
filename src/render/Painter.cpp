#include "render/Painter.h"
#include "render/IslandGeometry.h"
#include "app/Errors.h"
#include <d2d1effects.h>
#include <functional>
#include <numbers>
#include <vector>
#include <string>
#include <sstream>
#include <iomanip>
#include <locale>

namespace caecilia {
using Microsoft::WRL::ComPtr;
using namespace NotchIsland;
namespace {
float Unit(float value) { return std::clamp(value, PaintSpec::Zero, PaintSpec::Unit); }
// mingw-w64 declares the Shadow optimization property but omits its SDK enum.
// These are the Windows API enum ABI values, not visual measurements.
#ifdef __MINGW32__
constexpr UINT32 ShadowSpeed = 0;
constexpr UINT32 ShadowBalanced = 1;
#else
constexpr auto ShadowSpeed = D2D1_SHADOW_OPTIMIZATION_SPEED;
constexpr auto ShadowBalanced = D2D1_SHADOW_OPTIMIZATION_BALANCED;
#endif

// Command lists stay in body-local DIPs. Restore target/transform even on an HRESULT exception.
ComPtr<ID2D1CommandList> Record(ID2D1DeviceContext* context, const std::function<void()>& paint) {
    ComPtr<ID2D1Image> target;
    context->GetTarget(target.GetAddressOf());
    D2D1_MATRIX_3X2_F transform;
    context->GetTransform(&transform);
    ComPtr<ID2D1CommandList> commands;
    Check(context->CreateCommandList(commands.GetAddressOf()), "Create paint command list");
    context->SetTarget(commands.Get());
    context->SetTransform(D2D1::Matrix3x2F::Identity());
    context->BeginDraw();
    try {
        context->Clear(Color::Transparent);
        paint();
    } catch (...) {
        const HRESULT result = context->EndDraw();
        context->SetTarget(target.Get());
        context->SetTransform(transform);
        Check(result, "EndDraw interrupted command list");
        throw;
    }
    const HRESULT result = context->EndDraw();
    context->SetTarget(target.Get());
    context->SetTransform(transform);
    Check(result, "EndDraw command list");
    Check(commands->Close(), "Close paint command list");
    return commands;
}
ComPtr<ID2D1Effect> Shadow(ID2D1DeviceContext* context, ID2D1Image* image,
    float sigma, D2D1_COLOR_F color, bool speed) {
    ComPtr<ID2D1Effect> effect;
    Check(context->CreateEffect(CLSID_D2D1Shadow, effect.GetAddressOf()), "Create shadow effect");
    effect->SetInput(0, image);
    Check(effect->SetValue(D2D1_SHADOW_PROP_BLUR_STANDARD_DEVIATION, sigma), "Set shadow sigma");
    Check(effect->SetValue(D2D1_SHADOW_PROP_COLOR,
        D2D1_VECTOR_4F{color.r, color.g, color.b, color.a}), "Set shadow color");
    Check(effect->SetValue(D2D1_SHADOW_PROP_OPTIMIZATION, speed ?
        ShadowSpeed : ShadowBalanced), "Set shadow quality");
    return effect;
}
ComPtr<ID2D1LinearGradientBrush> Gradient(ID2D1DeviceContext* context,
    D2D1_POINT_2F start, D2D1_POINT_2F end, D2D1_COLOR_F first, D2D1_COLOR_F last) {
    const D2D1_GRADIENT_STOP stops[]{{PaintSpec::Zero, first}, {PaintSpec::Unit, last}};
    ComPtr<ID2D1GradientStopCollection> collection;
    Check(context->CreateGradientStopCollection(stops, static_cast<UINT32>(std::size(stops)),
        collection.GetAddressOf()), "Create gradient stops");
    ComPtr<ID2D1LinearGradientBrush> brush;
    Check(context->CreateLinearGradientBrush(D2D1::LinearGradientBrushProperties(start, end),
        collection.Get(), brush.GetAddressOf()), "Create gradient brush");
    return brush;
}

class LayoutPainter {
public:
    LayoutPainter(ID2D1DeviceContext* context, ID2D1Factory* factory, TextStyles& text,
                  const Scene& scene, bool glowPass)
        : context_(context), factory_(factory), text_(text), scene_(scene), glowPass_(glowPass) {
        Check(context_->CreateSolidColorBrush(Color::Label, brush_.GetAddressOf()), "Create content brush");
        auto properties = D2D1::StrokeStyleProperties();
        properties.startCap = properties.endCap = properties.dashCap = D2D1_CAP_STYLE_ROUND;
        properties.lineJoin = D2D1_LINE_JOIN_ROUND;
        Check(factory_->CreateStrokeStyle(properties, nullptr, 0, round_.GetAddressOf()), "Create round stroke");
        auto joined = D2D1::StrokeStyleProperties();
        joined.lineJoin = D2D1_LINE_JOIN_ROUND;
        Check(factory_->CreateStrokeStyle(joined, nullptr, 0, join_.GetAddressOf()), "Create joined stroke");
        auto dashed = D2D1::StrokeStyleProperties();
        dashed.dashStyle = D2D1_DASH_STYLE_CUSTOM;
        // D2D dash lengths are multiples of stroke width; DropStroke keeps them in DIPs.
        const float dashes[]{PaintSpec::DropDash / PaintSpec::DropStroke, PaintSpec::DropGap / PaintSpec::DropStroke};
        Check(factory_->CreateStrokeStyle(dashed, dashes, static_cast<UINT32>(std::size(dashes)),
            dash_.GetAddressOf()), "Create drop-zone dash");
        // All LayoutPainter draws are recorded in body-local command lists;
        // do not inherit the previous frame's window-space translation.
        base_ = D2D1::Matrix3x2F::Identity();
    }
    void Draw(const ContentLayer& layer);
    std::vector<D2D1_RECT_F> bounds;
private:
    void Box(D2D1_RECT_F rect) { if (!glowPass_) bounds.push_back(rect); }
    ID2D1SolidColorBrush* Brush(D2D1_COLOR_F color) { brush_->SetColor(color); return brush_.Get(); }
    void Fill(D2D1_ROUNDED_RECT rect, D2D1_COLOR_F color) {
        Box(rect.rect);
        if (!glowPass_) context_->FillRoundedRectangle(rect, Brush(color));
    }
    void Text(std::wstring_view text, Typography::Role role, D2D1_RECT_F rect,
              D2D1_COLOR_F color, DWRITE_TEXT_ALIGNMENT alignment = DWRITE_TEXT_ALIGNMENT_LEADING,
              DWRITE_PARAGRAPH_ALIGNMENT vertical = DWRITE_PARAGRAPH_ALIGNMENT_NEAR) {
        Box(rect);
        if (glowPass_) return;
        auto layout = text_.Layout(text, role, rect, alignment, vertical);
        context_->DrawTextLayout({rect.left, rect.top}, layout.Get(), Brush(color), D2D1_DRAW_TEXT_OPTIONS_CLIP);
    }
    void Accent(D2D1_ROUNDED_RECT rect, bool vertical = false, bool active = true) {
        Box(rect.rect);
        if (glowPass_) {
            if (active) context_->FillRoundedRectangle(rect, Brush(Color::Core));
            return;
        }
        auto gradient = Gradient(context_, {rect.rect.left, rect.rect.top},
            vertical ? D2D1::Point2F(rect.rect.left, rect.rect.bottom) : D2D1::Point2F(rect.rect.right, rect.rect.top),
            Color::Aqua, Color::Sky);
        context_->FillRoundedRectangle(rect, gradient.Get());
    }
    void Bar(D2D1_ROUNDED_RECT rect, float fraction) {
        Fill(rect, Color::Track);
        rect.rect.right = rect.rect.left + (rect.rect.right - rect.rect.left) * Unit(fraction);
        Accent(rect);
    }
    void Ring(D2D1_RECT_F rect, float fraction, float stroke, D2D1_COLOR_F color, bool glow);
    void Header(unsigned tab);
    void Dormant();
    void Compact();
    void NowPlaying();
    void Telemetry();
    void Tray();
    void Hud(HudKind kind);
    ComPtr<ID2D1PathGeometry> Path(const std::function<void(ID2D1GeometrySink*)>& build);
    template <std::size_t N>
    static void Figure(ID2D1GeometrySink* sink, const std::array<D2D1_POINT_2F, N>& points,
                       bool filled, bool closed) {
        sink->BeginFigure(points[0], filled ? D2D1_FIGURE_BEGIN_FILLED : D2D1_FIGURE_BEGIN_HOLLOW);
        sink->AddLines(points.data() + 1, static_cast<UINT32>(N - 1));
        sink->EndFigure(closed ? D2D1_FIGURE_END_CLOSED : D2D1_FIGURE_END_OPEN);
    }
    void At(D2D1_RECT_F rect, D2D1_SIZE_F size) {
        At({rect.left + (rect.right - rect.left - size.width) * WindowSpec::Half,
            rect.top + (rect.bottom - rect.top - size.height) * WindowSpec::Half});
    }
    void At(D2D1_POINT_2F origin) {
        context_->SetTransform(D2D1::Matrix3x2F::Translation(origin.x, origin.y) * base_);
    }
    void Reset() { context_->SetTransform(base_); }
    void MediaGlyph(D2D1_RECT_F button, int kind);
    void HudGlyph(D2D1_RECT_F rect, HudKind kind);
    void FileGlyph(D2D1_POINT_2F origin, D2D1_COLOR_F color);
    ID2D1DeviceContext* context_;
    ID2D1Factory* factory_;
    TextStyles& text_;
    const Scene& scene_;
    bool glowPass_;
    ComPtr<ID2D1SolidColorBrush> brush_;
    ComPtr<ID2D1StrokeStyle> round_;
    ComPtr<ID2D1StrokeStyle> join_;
    ComPtr<ID2D1StrokeStyle> dash_;
    D2D1_MATRIX_3X2_F base_{};
};

void LayoutPainter::Ring(D2D1_RECT_F rect, float fraction, float stroke, D2D1_COLOR_F color, bool glow) {
    Box(rect);
    const auto center = D2D1::Point2F((rect.left + rect.right) * WindowSpec::Half,
                                    (rect.top + rect.bottom) * WindowSpec::Half);
    const float radius = (rect.right - rect.left - stroke) * WindowSpec::Half;
    if (!glowPass_) context_->DrawEllipse(D2D1::Ellipse(center, radius, radius), Brush(Color::Track), stroke);
    fraction = Unit(fraction);
    if (fraction <= PaintSpec::Zero) return;
    // Only Sky accents feed the Sky glow mask; Focus has no glow token.
    if (glowPass_ && !glow) return;
    const auto ink = Brush(glowPass_ ? Color::Core : color);
    if (fraction == PaintSpec::Unit) {
        context_->DrawEllipse(D2D1::Ellipse(center, radius, radius), ink, stroke);
        return;
    }
    const float angle = fraction * (std::numbers::pi_v<float> / WindowSpec::Half);
    ComPtr<ID2D1PathGeometry> path;
    Check(factory_->CreatePathGeometry(path.GetAddressOf()), "Create ring path");
    ComPtr<ID2D1GeometrySink> sink;
    Check(path->Open(sink.GetAddressOf()), "Open ring path");
    sink->BeginFigure({center.x, center.y - radius}, D2D1_FIGURE_BEGIN_HOLLOW);
    sink->AddArc(D2D1::ArcSegment({center.x + std::sin(angle) * radius, center.y - std::cos(angle) * radius},
        {radius, radius}, PaintSpec::Zero, D2D1_SWEEP_DIRECTION_CLOCKWISE,
        fraction > WindowSpec::Half ? D2D1_ARC_SIZE_LARGE : D2D1_ARC_SIZE_SMALL));
    sink->EndFigure(D2D1_FIGURE_END_OPEN);
    Check(sink->Close(), "Close ring path");
    context_->DrawGeometry(path.Get(), ink, stroke, round_.Get());
}

std::wstring Integer(float value) { return std::to_wstring(static_cast<unsigned>(std::lround(value))); }
std::wstring Time(unsigned seconds) {
    const auto remainder = std::to_wstring(seconds % Demo::SecondsPerMinute);
    return std::to_wstring(seconds / Demo::SecondsPerMinute) + L":" +
        (remainder.size() == 1 ? L"0" : L"") + remainder;
}
std::wstring Decimal(float value) {
    std::wostringstream stream;
    stream.imbue(std::locale::classic());
    stream << std::fixed << std::setprecision(1) << value;
    return stream.str();
}
float EaseWave(float x) {
    // Invert the CSS cubic's x coordinate, then evaluate y. Iteration count is
    // numerical precision, not a visual token or an alternative easing curve.
    const auto cubic = [](float t, float a, float b) {
        const float u = PaintSpec::Unit - t;
        return 3 * u * u * t * a + 3 * u * t * t * b + t * t * t;
    };
    float low = PaintSpec::Zero, high = PaintSpec::Unit;
    for (unsigned iteration = 0; iteration < 24; ++iteration) {
        const float mid = (low + high) * WindowSpec::Half;
        if (cubic(mid, Motion::WaveBezier[0], Motion::WaveBezier[2]) < x) low = mid;
        else high = mid;
    }
    return cubic((low + high) * WindowSpec::Half, Motion::WaveBezier[1], Motion::WaveBezier[3]);
}

ComPtr<ID2D1PathGeometry> LayoutPainter::Path(const std::function<void(ID2D1GeometrySink*)>& build) {
    ComPtr<ID2D1PathGeometry> path;
    Check(factory_->CreatePathGeometry(path.GetAddressOf()), "Create glyph path");
    ComPtr<ID2D1GeometrySink> sink;
    Check(path->Open(sink.GetAddressOf()), "Open glyph path");
    build(sink.Get());
    Check(sink->Close(), "Close glyph path");
    return path;
}
void LayoutPainter::MediaGlyph(D2D1_RECT_F button, int kind) {
    Box(button);
    if (glowPass_) return;
    At(button, kind == 0 ? Glyph::PlaySize : Glyph::SkipSize);
    if (kind == 0 && scene_.mediaActive) {
        for (const auto& rect : Glyph::Pause) context_->FillRoundedRectangle(rect, Brush(Color::Label));
    } else {
        auto path = Path([&](ID2D1GeometrySink* sink) {
            if (kind < 0) {
                Figure(sink, Glyph::PrevLeft, true, true);
                Figure(sink, Glyph::PrevRight, true, true);
            } else if (kind > 0) {
                Figure(sink, Glyph::NextLeft, true, true);
                Figure(sink, Glyph::NextRight, true, true);
            } else Figure(sink, Glyph::Play, true, true);
        });
        context_->FillGeometry(path.Get(), Brush(Color::Label));
    }
    Reset();
}
void LayoutPainter::HudGlyph(D2D1_RECT_F rect, HudKind kind) {
    Box(rect);
    if (glowPass_) return;
    At(rect, Glyph::HudSize);
    if (kind == HudKind::Brightness) {
        context_->FillEllipse(Glyph::Sun, Brush(Color::Label));
        D2D1_MATRIX_3X2_F translated;
        context_->GetTransform(&translated);
        for (float angle : Glyph::SunRayAngles) {
            context_->SetTransform(D2D1::Matrix3x2F::Rotation(angle, Glyph::Sun.point) * translated);
            context_->DrawLine(Glyph::SunRay[0], Glyph::SunRay[1], Brush(Color::Label), Glyph::SunStroke, round_.Get());
        }
    } else {
        auto speaker = Path([](ID2D1GeometrySink* sink) { Figure(sink, Glyph::Speaker, true, true); });
        context_->FillGeometry(speaker.Get(), Brush(Color::Label));
        auto waves = Path([](ID2D1GeometrySink* sink) {
            for (const auto& arc : Glyph::SpeakerArcs) {
                sink->BeginFigure(arc.start, D2D1_FIGURE_BEGIN_HOLLOW);
                sink->AddArc(arc.segment);
                sink->EndFigure(D2D1_FIGURE_END_OPEN);
            }
        });
        context_->DrawGeometry(waves.Get(), Brush(Color::Label), Glyph::SpeakerStroke, round_.Get());
    }
    Reset();
}
void LayoutPainter::FileGlyph(D2D1_POINT_2F origin, D2D1_COLOR_F color) {
    Box({origin.x, origin.y, origin.x + Glyph::FileSize.width, origin.y + Glyph::FileSize.height});
    if (glowPass_) return;
    At(origin);
    auto file = Path([](ID2D1GeometrySink* sink) {
        Figure(sink, Glyph::FileOutline, false, true);
        Figure(sink, Glyph::FileFold, false, false);
    });
    context_->DrawGeometry(file.Get(), Brush(color), Glyph::FileStroke, join_.Get());
    Reset();
}
void LayoutPainter::Header(unsigned tab) {
    using namespace ExpandedNowPlaying;
    Fill(TabTrack, Color::Fill);
    auto pill = TabSel;
    const float width = pill.rect.right - pill.rect.left;
    pill.rect.left = scene_.tabPosition;
    pill.rect.right = pill.rect.left + width;
    Fill(pill, Color::Pill);
    const D2D1_RECT_F tabs[]{Tab0, Tab1, Tab2};
    for (unsigned i = 0; i < std::size(tabs); ++i) {
        Text(Demo::TabLabels[i], Typography::Role::TabClock, tabs[i], i == tab ? Color::Label : Color::Label2,
            DWRITE_TEXT_ALIGNMENT_CENTER, DWRITE_PARAGRAPH_ALIGNMENT_CENTER);
    }
    // HTML uses the local clock; no timer is created here just to update it.
    SYSTEMTIME now{};
    GetLocalTime(&now);
    const auto hours = std::to_wstring(now.wHour);
    const auto minutes = std::to_wstring(now.wMinute);
    Text((hours.size() == 1 ? L"0" : L"") + hours + L":" + (minutes.size() == 1 ? L"0" : L"") + minutes,
        Typography::Role::TabClock, Clock, Color::Label2, DWRITE_TEXT_ALIGNMENT_TRAILING, DWRITE_PARAGRAPH_ALIGNMENT_CENTER);
}
void LayoutPainter::Dormant() {
    const float opacity = scene_.mediaActive ? PaintSpec::DotOpacityActive : PaintSpec::DotOpacityIdle;
    if (glowPass_) {
        if (scene_.mediaActive) {
            auto color = Color::Core;
            color.a *= opacity;
            context_->FillRoundedRectangle(Idle::Status, Brush(color));
        }
        return;
    }
    Fill(Idle::Lens, Color::Lens);
    auto ring = Idle::Lens;
    const float inset = PaintSpec::LensStroke * WindowSpec::Half;
    ring.rect.left += inset;
    ring.rect.top += inset;
    ring.rect.right -= inset;
    ring.rect.bottom -= inset;
    ring.radiusX -= inset;
    ring.radiusY -= inset;
    context_->DrawRoundedRectangle(ring, Brush(Color::LensRing), PaintSpec::LensStroke);
    auto dot = Color::Aqua;
    dot.a *= opacity;
    Fill(Idle::Status, dot);
}
void LayoutPainter::Compact() {
    namespace C = NotchIsland::Compact;
    Fill(C::Art, Demo::ArtworkPlaceholder);
    Box(C::Wave);
    const D2D1_ROUNDED_RECT bars[]{C::Bar0, C::Bar1, C::Bar2, C::Bar3};
    for (std::size_t i = 0; i < std::size(bars); ++i) {
        auto bar = bars[i];
        Box(bar.rect); // Spec envelope as well as the actual scaled bar below.
        float scale = Motion::WaveMinimumScale;
        if (scene_.mediaActive && scene_.reducedMotion) {
            scale = Motion::ReducedMotionWaveHeight / (bar.rect.bottom - bar.rect.top);
        } else if (scene_.mediaActive) {
            const double milliseconds = scene_.time * Motion::MillisecondsPerSecond - Motion::WavePhaseMs[i];
            const float phase = static_cast<float>(std::fmod(milliseconds, Motion::WavePeriodsMs[i]) / Motion::WavePeriodsMs[i]);
            const float halfPhase = phase < WindowSpec::Half ? phase / WindowSpec::Half :
                (PaintSpec::Unit - phase) / WindowSpec::Half;
            scale += (PaintSpec::Unit - scale) * EaseWave(Unit(halfPhase));
        }
        const float center = (bar.rect.top + bar.rect.bottom) * WindowSpec::Half;
        const float height = (bar.rect.bottom - bar.rect.top) * scale * WindowSpec::Half;
        bar.rect.top = center - height;
        bar.rect.bottom = center + height;
        // CSS scaleY scales the corner's vertical radius too.
        bar.radiusY *= scale;
        Accent(bar, true, scene_.mediaActive);
    }
}
void LayoutPainter::NowPlaying() {
    using namespace ExpandedNowPlaying;
    const auto& track = Demo::Tracks[Demo::InitialTrack];
    const float progress = static_cast<float>(Demo::ElapsedSeconds) / track.durationSeconds;
    Fill(Art, Demo::ArtworkPlaceholder);
    Text(track.title, Typography::Role::MediaTitle, Title, Color::Label);
    Text(track.artist, Typography::Role::Artist, Artist, Color::Label2);
    Box(ScrubHit);
    Bar(ScrubTrack, progress);
    auto knob = Knob;
    const float displacement = (ScrubTrack.rect.right - ScrubTrack.rect.left) * progress;
    knob.rect.left += displacement;
    knob.rect.right += displacement;
    Fill(knob, Color::Label);
    Text(Time(Demo::ElapsedSeconds), Typography::Role::Timestamp, Elapsed, Color::Label3);
    Text(L"\u2212" + Time(track.durationSeconds - Demo::ElapsedSeconds), Typography::Role::Timestamp,
        Remain, Color::Label3, DWRITE_TEXT_ALIGNMENT_TRAILING);
    MediaGlyph(Prev.rect, -1);
    MediaGlyph(Play.rect, 0);
    MediaGlyph(Next.rect, 1);
    Box(Divider);
    if (!glowPass_) context_->FillRectangle(Divider, Brush(Color::Fill));
    const D2D1_RECT_F rings[]{CpuRing.rect, RamRing.rect};
    const D2D1_RECT_F labels[]{CpuLabel, RamLabel};
    for (std::size_t i = 0; i < std::size(rings); ++i) {
        Ring(rings[i], Demo::Telemetry[i] / Demo::Percent, PaintSpec::RingStroke, Color::Sky, true);
        Text(Integer(Demo::Telemetry[i]), Typography::Role::TelemetryValue, rings[i], Color::Label,
            DWRITE_TEXT_ALIGNMENT_CENTER, DWRITE_PARAGRAPH_ALIGNMENT_CENTER);
        Text(Demo::TelemetryLabels[i], Typography::Role::MicroLabel, labels[i], Color::Label3, DWRITE_TEXT_ALIGNMENT_CENTER);
    }
    Text(Demo::TelemetryLabels[2], Typography::Role::MicroLabel, GpuLabel, Color::Label2);
    Text(Integer(Demo::Telemetry[2]) + L"%", Typography::Role::Timestamp, GpuLabel, Color::Label2, DWRITE_TEXT_ALIGNMENT_TRAILING);
    Bar(GpuBar, Demo::Telemetry[2] / Demo::Percent);
}
void LayoutPainter::Telemetry() {
    using namespace ExpandedTelemetry;
    const D2D1_RECT_F labels[]{Row0Label, Row1Label, Row2Label, Row3Label};
    const D2D1_RECT_F values[]{Row0Value, Row1Value, Row2Value, Row3Value};
    const D2D1_ROUNDED_RECT bars[]{Row0Bar, Row1Bar, Row2Bar, Row3Bar};
    const std::wstring strings[]{Integer(Demo::Telemetry[0]) + L"%",
        Decimal(Demo::Telemetry[1] * Demo::RamGbPerPercent) + L" / " + Integer(Demo::RamTotalGb) + L" GB",
        Integer(Demo::Telemetry[2]) + L"%", Decimal(Demo::Telemetry[3] * Demo::NetMbPerSecondPerPercent) + L" MB/s"};
    for (std::size_t i = 0; i < std::size(labels); ++i) {
        Text(Demo::TelemetryLabels[i], Typography::Role::MicroLabel, labels[i], Color::Label2);
        Text(strings[i], Typography::Role::TelemetryValue, values[i], Color::Label, DWRITE_TEXT_ALIGNMENT_TRAILING);
        Bar(bars[i], Demo::Telemetry[i] / Demo::Percent);
    }
}
void LayoutPainter::Tray() {
    using namespace ExpandedDropTray;
    Fill(Drop, Color::Fill);
    if (!glowPass_) {
        auto border = Drop;
        const float inset = PaintSpec::DropStroke * WindowSpec::Half;
        border.rect.left += inset;
        border.rect.top += inset;
        border.rect.right -= inset;
        border.rect.bottom -= inset;
        context_->DrawRoundedRectangle(border, Brush(Color::Label3), PaintSpec::DropStroke, dash_.Get());
    }
    const D2D1_ROUNDED_RECT tiles[]{Tile0, Tile1, Tile2};
    for (std::size_t i = 0; i < std::size(tiles); ++i) {
        Fill(tiles[i], Color::Fill);
        const auto& rect = tiles[i].rect;
        FileGlyph({rect.left + TileIconOffset.x, rect.top + TileIconOffset.y}, i == 0 ? Color::Sky : Color::Label2);
        const D2D1_RECT_F name{rect.left + TileNameOffset.x, rect.top + TileNameOffset.y,
            rect.right - TileNameRightInset, rect.top + TileNameOffset.y + Typography::TabClock.lineHeight};
        Text(Demo::FileNames[i], Typography::Role::TabClock, name, Color::Label);
    }
}
void LayoutPainter::Hud(HudKind kind) {
    if (kind == HudKind::Pomodoro) {
        Ring(HudPomodoro::Ring.rect, static_cast<float>(Demo::PomodoroLeftSeconds) / Demo::PomodoroTotalSeconds,
            PaintSpec::PomodoroStroke, Color::Focus, false);
        Text(Demo::PomodoroLabel, Typography::Role::HudValue, HudPomodoro::Label, Color::Label);
        Text(Time(Demo::PomodoroLeftSeconds), Typography::Role::HudValue, HudPomodoro::Time, Color::Focus,
            DWRITE_TEXT_ALIGNMENT_TRAILING);
    } else {
        HudGlyph(HudLevel::Icon, kind);
        const float level = kind == HudKind::Brightness ? Demo::Brightness : Demo::Volume;
        Bar(HudLevel::Track, level / Demo::Percent);
        Text(Integer(level), Typography::Role::HudValue, HudLevel::Value, Color::Label, DWRITE_TEXT_ALIGNMENT_TRAILING);
    }
}
void LayoutPainter::Draw(const ContentLayer& layer) {
    // Record() establishes identity after construction, so capture it here.
    context_->GetTransform(&base_);
    switch (layer.state) {
    case IslandState::Dormant: Dormant(); break;
    case IslandState::Compact: Compact(); break;
    case IslandState::Expanded:
        Header(layer.tab);
        switch (layer.tab) {
        case 0: NowPlaying(); break;
        case 1: Telemetry(); break;
        default: Tray(); break;
        }
        break;
    case IslandState::Hud: Hud(layer.hud); break;
    }
    Reset();
}

} // namespace

void Painter::Initialize() { text_.Initialize(); }

void Painter::Draw(ID2D1DeviceContext* context, IslandGeometry& geometry, const Scene& scene) {
    context->SetTextAntialiasMode(D2D1_TEXT_ANTIALIAS_MODE_GRAYSCALE);
    ComPtr<ID2D1SolidColorBrush> brush;
    Check(context->CreateSolidColorBrush(Color::Core, brush.GetAddressOf()), "Create surface brush");
    const auto& geom = scene.geometry;
    ComPtr<ID2D1Device> device;
    context->GetDevice(device.GetAddressOf());
    if (device.Get() != shadowDevice_.Get()) {
        shadow_.Reset();
        shadowDevice_ = device;
    }
    if (Unit(geom.shadow) > PaintSpec::Zero) {
        const bool same = shadow_ && geom.width == shadowGeometry_.width &&
            geom.height == shadowGeometry_.height && geom.bottomRadius == shadowGeometry_.bottomRadius &&
            geom.earRadius == shadowGeometry_.earRadius;
        if (!same) {
            auto mask = Record(context, [&] { context->FillGeometry(geometry.Path(), brush.Get()); });
            shadow_ = Shadow(context, mask.Get(), PaintSpec::ShadowBlur, Color::Shadow, scene.animating);
        }
        if (!same || geom.shadow != shadowGeometry_.shadow) {
            auto color = Color::Shadow;
            color.a *= Unit(geom.shadow);
            Check(shadow_->SetValue(D2D1_SHADOW_PROP_COLOR,
                D2D1_VECTOR_4F{color.r, color.g, color.b, color.a}), "Set sprung shadow alpha");
        }
        if (!same || scene.animating != shadowAnimating_) {
            Check(shadow_->SetValue(D2D1_SHADOW_PROP_OPTIMIZATION, scene.animating ?
                ShadowSpeed : ShadowBalanced), "Set sprung shadow quality");
            Check(shadow_->SetValue(D2D1_PROPERTY_CACHED, static_cast<BOOL>(!scene.animating)), "Cache settled shadow");
        }
        shadowGeometry_ = geom;
        shadowAnimating_ = scene.animating;
    }

    std::vector<D2D1_RECT_F> bounds;
    auto content = [&](const ContentLayer& layer, bool collectBounds) -> ComPtr<ID2D1Image> {
        LayoutPainter glowPainter(context, geometry.Factory(), text_, scene, true);
        auto glowMask = Record(context, [&] { glowPainter.Draw(layer); });
        auto glow = Shadow(context, glowMask.Get(), PaintSpec::GlowSigma, Color::Glow, scene.animating);
        LayoutPainter painter(context, geometry.Factory(), text_, scene, false);
        auto commands = Record(context, [&] {
            context->DrawImage(glow.Get(), PaintSpec::GlowOffset);
            painter.Draw(layer);
        });
        if (collectBounds) bounds = std::move(painter.bounds);
        if (scene.reducedMotion || layer.blur <= PaintSpec::Zero) return commands;
        ComPtr<ID2D1Effect> blur;
        Check(context->CreateEffect(CLSID_D2D1GaussianBlur, blur.GetAddressOf()), "Create content blur");
        blur->SetInput(0, commands.Get());
        Check(blur->SetValue(D2D1_GAUSSIANBLUR_PROP_STANDARD_DEVIATION, layer.blur), "Set content blur");
        ComPtr<ID2D1Image> image;
        blur->GetOutput(image.GetAddressOf());
        return image;
    };
    ComPtr<ID2D1Image> outgoing;
    if (scene.hasOutgoing && scene.outgoing.opacity > PaintSpec::Zero) outgoing = content(scene.outgoing, false);
    auto incoming = content(scene.incoming, true);
    const float offset = (WindowSpec::Width - geom.width) * WindowSpec::Half;
    auto rimBottom = Color::Rim;
    rimBottom.a = PaintSpec::RimBottomAlpha;
    auto rim = Gradient(context, {PaintSpec::Zero, PaintSpec::Zero}, {PaintSpec::Zero, geom.height}, Color::Rim, rimBottom);
    rim->SetOpacity(Unit(geom.rim));
    const float mix = Unit(geom.surfaceMix);
    const auto lerp = [mix](float a, float b) { return a + (b - a) * mix; };
    const D2D1_COLOR_F surface{lerp(Color::Core.r, Color::Obsidian.r), lerp(Color::Core.g, Color::Obsidian.g),
        lerp(Color::Core.b, Color::Obsidian.b), lerp(Color::Core.a, Color::Obsidian.a)};
    auto fpsText = scene.animating ? std::to_wstring(static_cast<unsigned>(std::lround((std::max)(PaintSpec::Zero, scene.fps)))) + L" FPS" : DebugHud::IdleText;
    ComPtr<IDWriteTextLayout> fps;
    if (scene.fpsVisible) fps = text_.Layout(fpsText, DebugHud::TextRole, DebugHud::FpsRect,
        DebugHud::Alignment, DWRITE_PARAGRAPH_ALIGNMENT_NEAR, DebugHud::Tabular);

    context->SetTransform(D2D1::Matrix3x2F::Identity());
    context->BeginDraw();
    context->Clear(Color::Transparent);
    context->SetTransform(D2D1::Matrix3x2F::Translation(offset, WindowSpec::Top));
    if (shadow_ && Unit(geom.shadow) > PaintSpec::Zero) context->DrawImage(shadow_.Get(), PaintSpec::ShadowOffset);
    brush->SetColor(surface);
    context->FillGeometry(geometry.Path(), brush.Get());
    const auto clip = D2D1::LayerParameters1(D2D1::InfiniteRect(), geometry.Path());
    context->PushLayer(clip, nullptr);
    if (Unit(geom.rim) > PaintSpec::Zero) {
        // DPI multiplication is only pixel snapping of the stroke origin, never
        // a second scaling of drawing geometry (SetDpi owns that conversion).
        float dpiX{}, dpiY{};
        context->GetDpi(&dpiX, &dpiY);
        const float snappedX = std::round(offset * dpiX / WindowSpec::DpiBase) * WindowSpec::DpiBase / dpiX;
        const float snappedY = std::round(WindowSpec::Top * dpiY / WindowSpec::DpiBase) * WindowSpec::DpiBase / dpiY;
        context->SetTransform(D2D1::Matrix3x2F::Translation(snappedX, snappedY));
        context->DrawGeometry(geometry.Path(), rim.Get(), PaintSpec::RimStroke);
        context->SetTransform(D2D1::Matrix3x2F::Translation(offset, WindowSpec::Top));
    }
    // DESIGN 5.1: content origin x = (W_current - W_target) / 2 keeps layouts centered.
    const auto contentX = [&](const ContentLayer& layer) {
        return (geom.width - GeometryFor(layer.state).width) * WindowSpec::Half;
    };
    auto drawContent = [&](ID2D1Image* image, const ContentLayer& layer) {
        auto fade = D2D1::LayerParameters1();
        fade.opacity = Unit(layer.opacity);
        context->PushLayer(fade, nullptr);
        context->DrawImage(image, D2D1::Point2F(contentX(layer), PaintSpec::Zero));
        context->PopLayer();
    };
    if (outgoing) drawContent(outgoing.Get(), scene.outgoing);
    drawContent(incoming.Get(), scene.incoming);
    for (auto& rect : bounds) {
        rect.left += contentX(scene.incoming);
        rect.right += contentX(scene.incoming);
    }
    if (fps) {
        brush->SetColor(scene.animating ? DebugHud::ActiveColor : DebugHud::IdleColor);
        context->DrawTextLayout({DebugHud::FpsRect.left, DebugHud::FpsRect.top}, fps.Get(), brush.Get(), D2D1_DRAW_TEXT_OPTIONS_CLIP);
    }
    context->PopLayer();
    if (scene.bounds) {
        brush->SetColor(Color::Aqua);
        bounds.push_back({PaintSpec::Zero, PaintSpec::Zero, geom.width, geom.height});
        bounds.push_back({-geom.earRadius, PaintSpec::Zero, PaintSpec::Zero, geom.earRadius});
        bounds.push_back({geom.width, PaintSpec::Zero, geom.width + geom.earRadius, geom.earRadius});
        bounds.push_back(DebugHud::FpsRect);
        for (const auto& rect : bounds) context->DrawRectangle(rect, brush.Get(), PaintSpec::DebugStroke);
    }
    Check(context->EndDraw(), "EndDraw island scene");
}
} // namespace caecilia

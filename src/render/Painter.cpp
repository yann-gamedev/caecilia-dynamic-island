#include "render/Painter.h"
#include "render/IslandGeometry.h"
#include "app/Errors.h"
#include "spec/NotchIslandSpec.h"
#include <d2d1effects.h>
#include <wrl/client.h>

namespace caecilia {
using Microsoft::WRL::ComPtr;
using namespace NotchIsland;

void Painter::Initialize() { text_.Initialize(); }

void Painter::Draw(ID2D1DeviceContext* context, IslandGeometry& geometry,
                   bool bounds, bool fps, bool mediaActive) {
    ComPtr<ID2D1SolidColorBrush> brush;
    Check(context->CreateSolidColorBrush(Color::Core, brush.GetAddressOf()), "CreateSolidColorBrush");
    const float opacity = mediaActive ? PaintSpec::DotOpacityActive : PaintSpec::DotOpacityIdle;
    const float offset = (WindowSpec::Width - kGeomIdle.width) * WindowSpec::Half;
    ComPtr<IDWriteTextLayout> idleText;
    if (fps) { idleText = text_.IdleLayout(); }

    ComPtr<ID2D1Effect> glow;
    if (mediaActive) {
        // Record a local-space alpha mask, not a manually DPI-scaled bitmap.
        // The command list feeds a Gaussian shadow effect for the status glow only.
        ComPtr<ID2D1Image> target;
        context->GetTarget(target.GetAddressOf());
        ComPtr<ID2D1CommandList> mask;
        Check(context->CreateCommandList(mask.GetAddressOf()), "CreateCommandList glow");
        context->SetTarget(mask.Get());
        context->SetTransform(D2D1::Matrix3x2F::Identity());
        context->BeginDraw();
        context->Clear(Color::Transparent);
        brush->SetColor(Color::Core);
        brush->SetOpacity(opacity);
        context->FillRoundedRectangle(Idle::Status, brush.Get());
        const HRESULT maskResult = context->EndDraw();
        context->SetTarget(target.Get());
        Check(maskResult, "EndDraw glow mask");
        Check(mask->Close(), "Close glow mask");
        Check(context->CreateEffect(CLSID_D2D1Shadow, glow.GetAddressOf()), "CreateEffect glow");
        glow->SetInput(0, mask.Get());
        Check(glow->SetValue(D2D1_SHADOW_PROP_BLUR_STANDARD_DEVIATION, PaintSpec::GlowSigma), "Glow sigma");
        const D2D1_VECTOR_4F glowColor{Color::Glow.r, Color::Glow.g, Color::Glow.b, Color::Glow.a};
        Check(glow->SetValue(D2D1_SHADOW_PROP_COLOR, glowColor), "Glow color");
    }

    context->SetTransform(D2D1::Matrix3x2F::Identity());
    context->BeginDraw();
    context->Clear(Color::Transparent);
    context->SetTransform(D2D1::Matrix3x2F::Translation(offset, WindowSpec::Top));
    brush->SetOpacity(PaintSpec::Unit);
    brush->SetColor(Color::Core);
    context->FillGeometry(geometry.Path(), brush.Get());

    // Contents never leak into the transparent envelope; no surface rim/shadow
    // in Dormant. PushLayer is also shared with future layout rendering.
    const auto layer = D2D1::LayerParameters1(D2D1::InfiniteRect(), geometry.Path());
    context->PushLayer(layer, nullptr);
    if (glow) { context->DrawImage(glow.Get()); }
    brush->SetColor(Color::Lens);
    context->FillRoundedRectangle(Idle::Lens, brush.Get());
    // Inset centerline so the complete 1-DIP lens ring stays in its layout rect.
    auto ring = Idle::Lens;
    const float inset = PaintSpec::LensStroke * WindowSpec::Half;
    ring.rect.left += inset;
    ring.rect.top += inset;
    ring.rect.right -= inset;
    ring.rect.bottom -= inset;
    ring.radiusX -= inset;
    ring.radiusY -= inset;
    brush->SetColor(Color::LensRing);
    context->DrawRoundedRectangle(ring, brush.Get(), PaintSpec::LensStroke);
    brush->SetColor(Color::Aqua);
    brush->SetOpacity(opacity);
    context->FillRoundedRectangle(Idle::Status, brush.Get());
    brush->SetOpacity(PaintSpec::Unit);
    if (fps) {
        brush->SetColor(DebugHud::IdleColor);
        context->DrawTextLayout(D2D1::Point2F(DebugHud::FpsRect.left, DebugHud::FpsRect.top),
            idleText.Get(), brush.Get(), D2D1_DRAW_TEXT_OPTIONS_CLIP);
    }
    context->PopLayer();
    if (bounds) {
        brush->SetColor(Color::Aqua);
        // Rects are intentionally unmasked so concave ear boxes remain visible.
        for (const auto& rect : {Idle::Body, Idle::LeftEar, Idle::RightEar,
                                 Idle::Lens.rect, Idle::Status.rect}) {
            context->DrawRectangle(rect, brush.Get(), PaintSpec::DebugStroke);
        }
        if (fps) { context->DrawRectangle(DebugHud::FpsRect, brush.Get(), PaintSpec::DebugStroke); }
    }
    Check(context->EndDraw(), "EndDraw Dormant");
}
} // namespace caecilia

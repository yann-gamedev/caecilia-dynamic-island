#include "render/IslandGeometry.h"
#include "input/ClickGesture.h"
#include "app/Errors.h"
#include <cmath>
#include <cstdio>
#include <exception>

namespace {
void Require(bool condition, const char* message) {
    if (!condition) { throw std::runtime_error(message); }
}
}
int main() {
    try {
        using namespace NotchIsland;
        caecilia::IslandGeometry geometry;
        geometry.Initialize();
        Require(geometry.Contains(D2D1::Point2F(kGeomIdle.width * WindowSpec::Half,
                kGeomIdle.height * WindowSpec::Half)), "Body center must hit");
        Require(!geometry.Contains(D2D1::Point2F(-kGeomIdle.earRadius, kGeomIdle.height)),
                "Transparent left envelope must not hit");
        Require(!geometry.Contains(D2D1::Point2F(kGeomIdle.width + kGeomIdle.earRadius,
                kGeomIdle.height)), "Transparent right envelope must not hit");
        Require(geometry.Contains(D2D1::Point2F(Idle::Status.rect.left, Idle::Status.rect.top)),
                "Status layout must be inside island");
        D2D1_RECT_F bounds{};
        caecilia::Check(geometry.Path()->GetBounds(nullptr, &bounds), "GetBounds");
        Require(bounds.left == -kGeomIdle.earRadius && bounds.top == WindowSpec::Top &&
                bounds.right == kGeomIdle.width + kGeomIdle.earRadius && bounds.bottom == kGeomIdle.height,
                "Dormant geometry envelope differs from spec");
        for (const auto& g : {kGeomIdle, kGeomCompact, kGeomExpanded, kGeomHud}) {
            Microsoft::WRL::ComPtr<ID2D1PathGeometry> path;
            caecilia::Check(BuildIslandGeometry(geometry.Factory(), g.width, g.height,
                g.bottomRadius, g.earRadius, path.GetAddressOf()), "Build all spec geometry");
            Require(path != nullptr, "Missing path");
        }
        // Edge cases mandated by the recipe, without introducing visual tokens.
        Microsoft::WRL::ComPtr<ID2D1PathGeometry> noEar;
        caecilia::Check(BuildIslandGeometry(geometry.Factory(), kGeomIdle.width,
            kGeomIdle.height, kGeomIdle.bottomRadius, PaintSpec::Zero, noEar.GetAddressOf()), "Zero ear");
        // M1.1: hit-test transform equals Painter's snapped body origin at 100/150/200%.
        using caecilia::BodyOrigin; using caecilia::BodyPointFromClient;
        for (const float dpi : {96.0f, 144.0f, 192.0f}) {
            const float scale = dpi / WindowSpec::DpiBase;
            for (const float width : {kGeomIdle.width, kGeomIdle.width + 3.3f, kGeomExpanded.width - 0.7f}) {
                const auto origin = BodyOrigin(width, dpi, dpi);
                const float px = origin.x * scale;
                Require(std::fabs(px - std::round(px)) < 1e-3f, "Body origin must land on a device pixel");
                const auto zero = BodyPointFromClient({px, origin.y * scale}, width, dpi);
                Require(std::fabs(zero.x) < 1e-4f && std::fabs(zero.y) < 1e-4f, "Hit transform differs from paint origin");
            }
            const auto client = [&](float x, float y) {
                const auto origin = BodyOrigin(kGeomIdle.width, dpi, dpi);
                return D2D1::Point2F((x + origin.x) * scale, (y + origin.y) * scale);
            };
            const auto hits = [&](float x, float y) {
                return geometry.Contains(BodyPointFromClient(client(x, y), kGeomIdle.width, dpi));
            };
            const float mid = kGeomIdle.width * WindowSpec::Half;
            Require(hits(mid, kGeomIdle.height - 0.5f), "Inside bottom edge must hit");
            Require(!hits(mid, kGeomIdle.height + 0.5f), "Below bottom edge must pass through");
            Require(!hits(-kGeomIdle.earRadius, kGeomIdle.height), "Left envelope corner must pass through");
            using namespace ExpandedNowPlaying;
            const auto center = [](D2D1_RECT_F r) { return D2D1::Point2F((r.left + r.right) * 0.5f, (r.top + r.bottom) * 0.5f); };
            using caecilia::ClickTarget; using caecilia::IslandState; using caecilia::TargetAt;
            const float w = kGeomExpanded.width;
            Require(TargetAt(center(Play.rect), IslandState::Expanded, 0, w, dpi, false) == ClickTarget::Play, "Play target");
            Require(TargetAt(center(Tab1), IslandState::Expanded, 0, w, dpi, false) == ClickTarget::Tab1, "Tab target");
            Require(TargetAt(center(Play.rect), IslandState::Expanded, 1, w, dpi, false) == ClickTarget::Background,
                "Hidden-tab controls must not hit");
        }
        using caecilia::ClickTarget;
        Require(caecilia::TargetAt({}, caecilia::IslandState::Compact, 0, kGeomCompact.width, 96, true) == ClickTarget::MiddlePlay,
            "Compact middle click");
        Require(caecilia::TargetAt({}, caecilia::IslandState::Dormant, 0, kGeomIdle.width, 96, true) == ClickTarget::None,
            "Dormant middle click is ignored");
        caecilia::ClickGesture click;
        click.Down(ClickTarget::Play, false);
        Require(click.Up(ClickTarget::Play, false), "Same-target release fires");
        Require(!click.Up(ClickTarget::Play, false), "Release without press does not fire");
        click.Down(ClickTarget::Play, false);
        Require(!click.Up(ClickTarget::Next, false), "Different-target release is ignored");
        click.Down(ClickTarget::None, false); // Press outside the silhouette.
        Require(!click.Up(ClickTarget::Background, false), "Outside press, inside release is ignored");
        click.Down(ClickTarget::Background, false);
        Require(!click.Up(ClickTarget::None, false), "Inside press, outside release is ignored");
        click.Down(ClickTarget::Tab0, false); click.Cancel();
        Require(!click.Up(ClickTarget::Tab0, false), "Cancelled capture does not fire");
        click.Down(ClickTarget::MiddlePlay, true);
        Require(!click.Up(ClickTarget::MiddlePlay, false), "Button mismatch is ignored");
        std::puts("PASS: geometry bounds, DPI hit transform, click gesture, and zero-ear construction");
        return 0;
    } catch (const std::exception& error) {
        std::fprintf(stderr, "%s\n", error.what());
        return 1;
    }
}

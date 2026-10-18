#include "render/IslandGeometry.h"
#include "app/Errors.h"
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
        std::puts("PASS: geometry bounds, hit testing, and zero-ear construction");
        return 0;
    } catch (const std::exception& error) {
        std::fprintf(stderr, "%s\n", error.what());
        return 1;
    }
}

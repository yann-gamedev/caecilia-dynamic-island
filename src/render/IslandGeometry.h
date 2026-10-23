#pragma once
#include <d2d1_1.h>
#include <wrl/client.h>
#include <cmath>
#include "spec/NotchIslandSpec.h"

namespace caecilia {
// DIP coordinates share the renderer's device-pixel rounding (DPI must be positive).
inline float SnapToPixel(float dip, float dpi) noexcept {
    return std::round(dip * dpi / NotchIsland::WindowSpec::DpiBase) * NotchIsland::WindowSpec::DpiBase / dpi;
}
inline D2D1_POINT_2F BodyOrigin(float width, float dpiX, float dpiY) noexcept {
    using namespace NotchIsland;
    return {SnapToPixel((WindowSpec::Width - width) * WindowSpec::Half, dpiX),
            SnapToPixel(WindowSpec::Top, dpiY)};
}
inline D2D1_POINT_2F BodyPointFromClient(D2D1_POINT_2F pixels, float width, float dpi) noexcept {
    const auto origin = BodyOrigin(width, dpi, dpi);
    const float scale = NotchIsland::WindowSpec::DpiBase / dpi;
    return {pixels.x * scale - origin.x, pixels.y * scale - origin.y};
}
class IslandGeometry final {
public:
    void Initialize();
    void Update(const NotchIsland::IslandGeom& value);
    [[nodiscard]] const NotchIsland::IslandGeom& Value() const noexcept { return value_; }
    [[nodiscard]] ID2D1Factory1* Factory() const noexcept { return factory_.Get(); }
    [[nodiscard]] ID2D1PathGeometry* Path() const noexcept { return path_.Get(); }
    [[nodiscard]] bool Contains(D2D1_POINT_2F point) const;
private:
    NotchIsland::IslandGeom value_{NotchIsland::kGeomIdle};
    Microsoft::WRL::ComPtr<ID2D1Factory1> factory_;
    Microsoft::WRL::ComPtr<ID2D1PathGeometry> path_;
};
} // namespace caecilia

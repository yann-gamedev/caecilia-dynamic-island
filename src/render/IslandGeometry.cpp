#include "render/IslandGeometry.h"
#include "app/Errors.h"

namespace caecilia {
void IslandGeometry::Initialize() {
    Check(D2D1CreateFactory(D2D1_FACTORY_TYPE_SINGLE_THREADED,
                           IID_PPV_ARGS(factory_.ReleaseAndGetAddressOf())), "D2D1CreateFactory");
    Update(NotchIsland::kGeomIdle);
}

void IslandGeometry::Update(const NotchIsland::IslandGeom& g) {
    if (!path_ || value_.width != g.width || value_.height != g.height ||
        value_.bottomRadius != g.bottomRadius || value_.earRadius != g.earRadius) {
        Microsoft::WRL::ComPtr<ID2D1PathGeometry> next;
        Check(NotchIsland::BuildIslandGeometry(factory_.Get(), g.width, g.height,
            g.bottomRadius, g.earRadius, next.GetAddressOf()), "BuildIslandGeometry");
        path_ = std::move(next);
    }
    value_ = g;
}

bool IslandGeometry::Contains(D2D1_POINT_2F point) const {
    BOOL contains = FALSE;
    Check(path_->FillContainsPoint(point, nullptr, &contains), "FillContainsPoint");
    return contains != FALSE;
}
} // namespace caecilia

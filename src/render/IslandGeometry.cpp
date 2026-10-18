#include "render/IslandGeometry.h"
#include "app/Errors.h"

namespace caecilia {
void IslandGeometry::Initialize() {
    Check(D2D1CreateFactory(D2D1_FACTORY_TYPE_SINGLE_THREADED,
                           IID_PPV_ARGS(factory_.ReleaseAndGetAddressOf())), "D2D1CreateFactory");
    const auto& g = NotchIsland::kGeomIdle;
    Check(NotchIsland::BuildIslandGeometry(factory_.Get(), g.width, g.height,
          g.bottomRadius, g.earRadius, path_.ReleaseAndGetAddressOf()), "BuildIslandGeometry");
}

bool IslandGeometry::Contains(D2D1_POINT_2F point) const {
    BOOL contains = FALSE;
    Check(path_->FillContainsPoint(point, nullptr, &contains), "FillContainsPoint");
    return contains != FALSE;
}
} // namespace caecilia

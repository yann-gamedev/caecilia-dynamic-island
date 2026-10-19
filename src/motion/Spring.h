#pragma once

#include "spec/NotchIslandSpec.h"
#include <array>

namespace caecilia {
class IslandSprings {
public:
    IslandSprings() noexcept;
    void SetTarget(NotchIsland::IslandGeom target, NotchIsland::SpringParams params) noexcept;
    void Advance(double elapsed) noexcept;
    void Snap() noexcept;
    bool Settled() const noexcept { return settled_; }
    NotchIsland::IslandGeom Value() const noexcept;
    // width, height, bottomRadius, earRadius, surfaceMix, rim, shadow.
    const std::array<NotchIsland::SpringValue, 7>& Values() const noexcept { return values_; }

private:
    std::array<NotchIsland::SpringValue, 7> values_{};
    NotchIsland::SpringParams params_ = NotchIsland::Spring::IdleToCompact;
    double accumulator_ = 0.0;
    bool settled_ = true;
};
} // namespace caecilia

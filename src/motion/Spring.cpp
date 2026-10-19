#include "motion/Spring.h"
#include <algorithm>
#include <cmath>
#include <limits>

namespace caecilia {
namespace {
std::array<float, 7> Components(const NotchIsland::IslandGeom& geometry) noexcept {
    return {geometry.width, geometry.height, geometry.bottomRadius, geometry.earRadius,
            geometry.surfaceMix, geometry.rim, geometry.shadow};
}
}

IslandSprings::IslandSprings() noexcept {
    const auto idle = Components(NotchIsland::kGeomIdle);
    for (std::size_t i = 0; i < values_.size(); ++i) {
        values_[i] = {idle[i], NotchIsland::PaintSpec::Zero, idle[i]};
    }
}

void IslandSprings::SetTarget(NotchIsland::IslandGeom target, NotchIsland::SpringParams params) noexcept {
    const auto components = Components(target);
    for (std::size_t i = 0; i < values_.size(); ++i) { values_[i].target = components[i]; }
    params_ = params;
    // Preserve both velocity and fractional elapsed time on interruption.
    settled_ = std::all_of(values_.begin(), values_.end(), [](const auto& value) {
        return value.x == value.target && value.v == NotchIsland::PaintSpec::Zero;
    });
}

void IslandSprings::Snap() noexcept {
    for (auto& value : values_) {
        value.x = value.target;
        value.v = NotchIsland::PaintSpec::Zero;
    }
    accumulator_ = 0.0;
    settled_ = true;
}

void IslandSprings::Advance(double elapsed) noexcept {
    if (settled_ || !std::isfinite(elapsed) || elapsed <= 0.0) { return; }
    using namespace NotchIsland;
    accumulator_ += (std::min)(elapsed, Motion::MaxFrameDelta);
    // Only compensate for floating-point roundoff at a fixed-step boundary.
    constexpr double rounding = std::numeric_limits<double>::epsilon() * Motion::MaxFrameDelta;
    while (accumulator_ + rounding >= Motion::FixedStep) {
        accumulator_ = (std::max)(0.0, accumulator_ - Motion::FixedStep);
        for (auto& value : values_) { StepSpring(value, params_, static_cast<float>(Motion::FixedStep)); }
        if (std::all_of(values_.begin(), values_.end(), IsSettled)) {
            for (auto& value : values_) {
                value.x = value.target;
                value.v = PaintSpec::Zero;
            }
            settled_ = true;
            accumulator_ = 0.0;
            break;
        }
    }
}

NotchIsland::IslandGeom IslandSprings::Value() const noexcept {
    return {values_[0].x, values_[1].x, values_[2].x, values_[3].x,
            values_[4].x, values_[5].x, values_[6].x};
}
} // namespace caecilia

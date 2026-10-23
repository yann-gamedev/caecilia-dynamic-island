#pragma once
#include "render/IslandGeometry.h"
#include "state/StateMachine.h"
#include <utility>

namespace caecilia {
enum class ClickTarget {
    None, Dormant, Compact, Background, Tab0, Tab1, Tab2,
    Previous, Play, Next, Scrub, File0, File1, File2, MiddlePlay
};

// Caller first checks the island silhouette. Controls use the incoming content transform.
inline ClickTarget TargetAt(D2D1_POINT_2F point, IslandState state, unsigned tab,
                            float width, float dpi, bool middle) noexcept {
    using namespace NotchIsland;
    if (middle) return state == IslandState::Compact ? ClickTarget::MiddlePlay : ClickTarget::None;
    if (state == IslandState::Dormant) return ClickTarget::Dormant;
    if (state == IslandState::Compact) return ClickTarget::Compact;
    if (state != IslandState::Expanded) return ClickTarget::None;
    point.x -= (width - kGeomExpanded.width) * WindowSpec::Half;
    const auto hit = [&](D2D1_RECT_F rect, bool snap = false) {
        if (snap) rect = {SnapToPixel(rect.left, dpi), SnapToPixel(rect.top, dpi),
                          SnapToPixel(rect.right, dpi), SnapToPixel(rect.bottom, dpi)};
        return point.x >= rect.left && point.x <= rect.right && point.y >= rect.top && point.y <= rect.bottom;
    };
    using namespace ExpandedNowPlaying;
    if (hit(Tab0)) return ClickTarget::Tab0;
    if (hit(Tab1)) return ClickTarget::Tab1;
    if (hit(Tab2)) return ClickTarget::Tab2;
    if (tab == 0) {
        if (hit(Prev.rect, true)) return ClickTarget::Previous;
        if (hit(Play.rect, true)) return ClickTarget::Play;
        if (hit(Next.rect, true)) return ClickTarget::Next;
        if (hit(ScrubHit)) return ClickTarget::Scrub;
    } else if (tab == 2) {
        if (hit(ExpandedDropTray::Tile0.rect, true)) return ClickTarget::File0;
        if (hit(ExpandedDropTray::Tile1.rect, true)) return ClickTarget::File1;
        if (hit(ExpandedDropTray::Tile2.rect, true)) return ClickTarget::File2;
    }
    return ClickTarget::Background;
}

class ClickGesture final {
public:
    void Down(ClickTarget target, bool middle) noexcept { target_ = target; middle_ = middle; }
    [[nodiscard]] bool Up(ClickTarget target, bool middle) noexcept {
        const auto pressed = std::exchange(target_, ClickTarget::None);
        return pressed != ClickTarget::None && pressed == target && middle_ == middle;
    }
    void Cancel() noexcept { target_ = ClickTarget::None; }
private:
    ClickTarget target_{ClickTarget::None};
    bool middle_{};
};
} // namespace caecilia

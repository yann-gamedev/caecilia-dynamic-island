#pragma once

#include "spec/NotchIslandSpec.h"
#include <optional>

namespace caecilia {
enum class IslandState { Dormant, Compact, Expanded, Hud };
enum class HudKind { Volume, Brightness, Pomodoro };

const NotchIsland::IslandGeom& GeometryFor(IslandState state) noexcept;
const NotchIsland::SpringParams& SpringFor(IslandState from, IslandState to) noexcept;

class StateMachine {
public:
    IslandState Current() const noexcept;
    HudKind Hud() const noexcept { return hud_; }
    // now is steady_clock time expressed in seconds, from the caller's epoch.
    // Demo base selection also releases pinned/hover expansion, but not an active HUD.
    void SetBase(IslandState base, double now) noexcept;
    void SetExpanded(bool expanded) noexcept { expanded_ = expanded; }
    void TriggerHud(HudKind kind, double now) noexcept;
    void PinHud(HudKind kind) noexcept;
    void DismissHud() noexcept;
    void Tick(double now) noexcept;
    std::optional<double> NextDeadline() const noexcept { return hudDeadline_; }

private:
    IslandState base_ = IslandState::Dormant;
    bool expanded_ = false;
    bool pinnedHud_ = false;
    HudKind hud_ = HudKind::Volume;
    std::optional<double> hudDeadline_;
};
} // namespace caecilia

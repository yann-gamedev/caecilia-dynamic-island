#pragma once

#include "spec/NotchIslandSpec.h"
#include <optional>

namespace caecilia {
enum class IslandState { Dormant = 1, Compact, Expanded, Hud };
enum class HudKind { Volume, Brightness, Pomodoro };

const NotchIsland::IslandGeom& GeometryFor(IslandState state) noexcept;
// Hover Swell (DESIGN 3.2) adds the swell deltas to the Dormant / Compact target.
NotchIsland::IslandGeom TargetGeometry(IslandState state, bool swell) noexcept;
const NotchIsland::SpringParams& SpringFor(IslandState from, IslandState to) noexcept;

// DESIGN 3.0: tampil = hud ? HUD : expanded ? Expanded : base.
class StateMachine {
public:
    IslandState Current() const noexcept;
    bool Swell() const noexcept;
    bool Pinned() const noexcept { return pinned_; }
    HudKind Hud() const noexcept { return hud_; }
    // now is steady_clock time expressed in seconds, from the caller's epoch.
    void SetBase(IslandState base, double now) noexcept;
    // Immediate pointer state; leaving clears suppressHover.
    void SetHovered(bool hovered) noexcept;
    // Delayed hover decision (hoverDelayMs after enter, CollapseDelayMs after leave).
    void SetHoverExpanded(bool expanded) noexcept;
    void Pin() noexcept { pinned_ = true; }
    // Unpin collapses at once; while the pointer is still inside, hover stays suppressed.
    void Unpin() noexcept;
    // Media source closed: Dormant at once and clear pin (D2).
    void CloseMedia(double now) noexcept;
    void TriggerHud(HudKind kind, double now) noexcept;
    void PinHud(HudKind kind) noexcept;
    void DismissHud() noexcept;
    void Tick(double now) noexcept;
    std::optional<double> NextDeadline() const noexcept { return hudDeadline_; }

private:
    bool Expanded() const noexcept { return pinned_ || (hoverExpanded_ && !suppressHover_); }
    IslandState base_ = IslandState::Dormant;
    bool pinned_ = false;
    bool hovered_ = false;
    bool hoverExpanded_ = false;
    bool suppressHover_ = false;
    bool pinnedHud_ = false;
    HudKind hud_ = HudKind::Volume;
    std::optional<double> hudDeadline_;
};
} // namespace caecilia

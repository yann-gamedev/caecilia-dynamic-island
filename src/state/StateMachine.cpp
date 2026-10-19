#include "state/StateMachine.h"

namespace caecilia {
const NotchIsland::IslandGeom& GeometryFor(IslandState state) noexcept {
    switch (state) {
    case IslandState::Dormant: return NotchIsland::kGeomIdle;
    case IslandState::Compact: return NotchIsland::kGeomCompact;
    case IslandState::Expanded: return NotchIsland::kGeomExpanded;
    case IslandState::Hud: return NotchIsland::kGeomHud;
    }
    return NotchIsland::kGeomIdle;
}

const NotchIsland::SpringParams& SpringFor(IslandState from, IslandState to) noexcept {
    // HUD precedence must win even when the other endpoint is Expanded.
    using namespace NotchIsland;
    if (to == IslandState::Hud) { return Spring::ToHud; }
    if (from == IslandState::Hud) { return Spring::FromHud; }
    if (to == IslandState::Expanded) { return Spring::ToExpanded; }
    if (from == IslandState::Expanded) { return Spring::Collapse; }
    if (to == IslandState::Compact) { return Spring::IdleToCompact; }
    return Spring::CompactToIdle;
}

IslandState StateMachine::Current() const noexcept {
    if (pinnedHud_ || hudDeadline_) { return IslandState::Hud; }
    if (expanded_) { return IslandState::Expanded; }
    return base_;
}

void StateMachine::SetBase(IslandState base, double now) noexcept {
    Tick(now);
    if (base != IslandState::Dormant && base != IslandState::Compact) { return; }
    base_ = base;
    expanded_ = false;
}

void StateMachine::TriggerHud(HudKind kind, double now) noexcept {
    pinnedHud_ = false;
    hud_ = kind;
    hudDeadline_ = now + NotchIsland::Motion::HudMs / NotchIsland::Motion::MillisecondsPerSecond;
}

void StateMachine::PinHud(HudKind kind) noexcept {
    hud_ = kind;
    pinnedHud_ = true;
    hudDeadline_.reset();
}

void StateMachine::DismissHud() noexcept {
    pinnedHud_ = false;
    hudDeadline_.reset();
}

void StateMachine::Tick(double now) noexcept {
    if (hudDeadline_ && now >= *hudDeadline_) { hudDeadline_.reset(); }
}
} // namespace caecilia

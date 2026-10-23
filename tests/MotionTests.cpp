#include "motion/Spring.h"
#include "state/StateMachine.h"
#include "input/HoverTracker.h"
#include "app/Clock.h"
#include <array>
#include <cmath>
#include <cstdio>
#include <exception>
#include <limits>
#include <stdexcept>

namespace {
using namespace caecilia;
using namespace NotchIsland;

void Require(bool condition, const char* message) {
    if (!condition) { throw std::runtime_error(message); }
}

std::array<float, 7> Components(const IslandGeom& g) {
    return {g.width, g.height, g.bottomRadius, g.earRadius, g.surfaceMix, g.rim, g.shadow};
}

void RequireSame(const IslandSprings& a, const IslandSprings& b, const char* message) {
    for (std::size_t i = 0; i < a.Values().size(); ++i) {
        Require(a.Values()[i].x == b.Values()[i].x &&
                a.Values()[i].v == b.Values()[i].v &&
                a.Values()[i].target == b.Values()[i].target, message);
    }
}

void RequireParams(IslandState from, IslandState to, SpringParams expected) {
    const auto& actual = SpringFor(from, to);
    Require(actual.stiffness == expected.stiffness && actual.damping == expected.damping &&
            actual.mass == expected.mass, "Incorrect transition spring / HUD precedence");
}

void Finish(IslandSprings& springs, const IslandGeom& target) {
    // Test timeout, not a visual or motion parameter.
    for (int frame = 0; frame < 1000 && !springs.Settled(); ++frame) {
        springs.Advance(Motion::MaxFrameDelta);
        for (const auto& value : springs.Values()) {
            Require(std::isfinite(value.x) && std::isfinite(value.v), "Spring must stay finite");
        }
    }
    Require(springs.Settled(), "Transition did not converge");
    Require(Components(springs.Value()) == Components(target), "Settled geometry must snap exactly");
    for (const auto& value : springs.Values()) {
        Require(value.x == value.target && value.v == PaintSpec::Zero,
                "Settled spring must have exact target and zero velocity");
    }
    const auto before = springs;
    springs.Advance(Motion::MaxFrameDelta);
    RequireSame(springs, before, "Settled springs must remain idle");
}

void TestTransitions() {
    IslandSprings springs;
    Require(springs.Settled(), "Initial springs must be settled");
    Require(Components(springs.Value()) == Components(kGeomIdle), "Initial geometry must be Dormant");
    RequireParams(IslandState::Dormant, IslandState::Compact, Spring::IdleToCompact);
    RequireParams(IslandState::Compact, IslandState::Dormant, Spring::CompactToIdle);
    for (const auto base : {IslandState::Dormant, IslandState::Compact}) {
        RequireParams(base, IslandState::Expanded, Spring::ToExpanded);
        RequireParams(IslandState::Expanded, base, Spring::Collapse);
    }
    for (const auto state : {IslandState::Dormant, IslandState::Compact, IslandState::Expanded}) {
        RequireParams(state, IslandState::Hud, Spring::ToHud);
        RequireParams(IslandState::Hud, state, Spring::FromHud);
    }
    RequireParams(IslandState::Hud, IslandState::Hud, Spring::ToHud);
    Require(Components(GeometryFor(IslandState::Dormant)) == Components(kGeomIdle) &&
            Components(GeometryFor(IslandState::Compact)) == Components(kGeomCompact) &&
            Components(GeometryFor(IslandState::Expanded)) == Components(kGeomExpanded) &&
            Components(GeometryFor(IslandState::Hud)) == Components(kGeomHud), "State geometry mapping");

    // Every distinct state pair exercises all six spring families and both collapse bases.
    for (const auto from : {IslandState::Dormant, IslandState::Compact, IslandState::Expanded, IslandState::Hud}) {
        for (const auto to : {IslandState::Dormant, IslandState::Compact, IslandState::Expanded, IslandState::Hud}) {
            if (from == to) { continue; }
            IslandSprings transition;
            transition.SetTarget(GeometryFor(from), SpringFor(IslandState::Dormant, from));
            Finish(transition, GeometryFor(from));
            transition.SetTarget(GeometryFor(to), SpringFor(from, to));
            Require(!transition.Settled(), "New target must wake spring animation");
            Finish(transition, GeometryFor(to));
        }
    }
}

void TestRetargetAndTiming() {
    IslandSprings springs;
    springs.SetTarget(kGeomExpanded, Spring::ToExpanded);
    springs.Advance(Motion::MaxFrameDelta);
    springs.Advance(Motion::FixedStep / 2.0);
    const auto before = springs.Values();
    Require(before[0].v != PaintSpec::Zero, "Retarget test requires motion");
    springs.SetTarget(kGeomIdle, Spring::Collapse);
    for (std::size_t i = 0; i < before.size(); ++i) {
        Require(springs.Values()[i].x == before[i].x && springs.Values()[i].v == before[i].v,
                "Retarget must preserve position and velocity");
    }
    auto expected = springs.Values();
    for (auto& value : expected) { StepSpring(value, Spring::Collapse, static_cast<float>(Motion::FixedStep)); }
    springs.Advance(Motion::FixedStep / 2.0);
    for (std::size_t i = 0; i < expected.size(); ++i) {
        Require(springs.Values()[i].x == expected[i].x && springs.Values()[i].v == expected[i].v,
                "Retarget must preserve fractional fixed-step accumulator");
    }
    Finish(springs, kGeomIdle);

    IslandSprings clamped;
    clamped.SetTarget(kGeomExpanded, Spring::ToExpanded);
    auto normal = clamped;
    clamped.Advance(Motion::MaxFrameDelta * 100.0);
    normal.Advance(Motion::MaxFrameDelta);
    RequireSame(clamped, normal, "Long frame must clamp to MaxFrameDelta");
    clamped.Advance(Motion::FixedStep);
    normal.Advance(Motion::FixedStep);
    RequireSame(clamped, normal, "Clamped time must not leave a backlog");

    const auto unchanged = normal;
    normal.Advance(0.0);
    normal.Advance(-Motion::FixedStep);
    normal.Advance(std::numeric_limits<double>::quiet_NaN());
    normal.Advance(std::numeric_limits<double>::infinity());
    RequireSame(normal, unchanged, "Invalid elapsed times must not corrupt springs");

    // Equal elapsed time at 60, 120, 144 and 240 Hz must perform identical fixed steps.
    for (const int hz : {60, 120, 144, 240}) {
        IslandSprings scheduled;
        IslandSprings fixed;
        scheduled.SetTarget(kGeomExpanded, Spring::ToExpanded);
        fixed.SetTarget(kGeomExpanded, Spring::ToExpanded);
        for (int frame = 0; frame < hz / 2; ++frame) { scheduled.Advance(1.0 / hz); }
        for (int step = 0; step < 120; ++step) { fixed.Advance(Motion::FixedStep); }
        RequireSame(scheduled, fixed, "Equivalent frame schedules must produce identical springs");
    }
}

void TestStateMachine() {
    constexpr double start = Motion::HudMs / Motion::MillisecondsPerSecond;
    constexpr double duration = Motion::HudMs / Motion::MillisecondsPerSecond;
    static_assert(static_cast<int>(IslandState::Dormant) == 1 && static_cast<int>(IslandState::Hud) == 4);
    StateMachine closed;
    closed.SetBase(IslandState::Compact, start);
    closed.SetHovered(true);
    closed.SetHoverExpanded(true);
    closed.CloseMedia(start);
    Require(closed.Current() == IslandState::Dormant && !closed.Swell(), "Close media must clear hover expansion");
    closed.SetHoverExpanded(true); // A previously armed enter timer must not reopen it.
    Require(closed.Current() == IslandState::Dormant, "Close media suppresses hover until exit");
    closed.SetHovered(false);
    closed.SetHovered(true);
    closed.SetHoverExpanded(true);
    Require(closed.Current() == IslandState::Expanded, "Hover works again after exit/re-entry");
    StateMachine state;
    Require(state.Current() == IslandState::Dormant && !state.NextDeadline(), "Initial state must be idle");
    state.SetBase(IslandState::Compact, start);
    Require(state.Current() == IslandState::Compact, "Compact base selection");
    state.Pin();
    Require(state.Current() == IslandState::Expanded && !state.NextDeadline(), "Pinned expansion must stay");
    state.Tick(start + duration);
    Require(state.Current() == IslandState::Expanded, "Expansion cannot expire without a leave decision");
    state.TriggerHud(HudKind::Volume, start);
    Require(state.Current() == IslandState::Hud && state.Hud() == HudKind::Volume, "HUD must override Expanded");
    const double firstDeadline = *state.NextDeadline();
    state.TriggerHud(HudKind::Brightness, start + Motion::FixedStep);
    Require(state.Hud() == HudKind::Brightness && *state.NextDeadline() == firstDeadline + Motion::FixedStep,
            "A new HUD event must replace kind and refresh expiry");
    state.Tick(firstDeadline);
    Require(state.Current() == IslandState::Hud, "Refreshed HUD cannot expire at the old deadline");
    state.Tick(*state.NextDeadline());
    Require(state.Current() == IslandState::Expanded && !state.NextDeadline(), "HUD expiry must restore expansion and clear timer");
    state.Unpin();
    Require(state.Current() == IslandState::Compact, "Unpin must restore Compact base");

    state.Pin();
    state.TriggerHud(HudKind::Pomodoro, start);
    state.SetBase(IslandState::Dormant, start + Motion::FixedStep);
    Require(state.Current() == IslandState::Hud && state.Hud() == HudKind::Pomodoro,
            "Base change must not dismiss an active HUD");
    Require(*state.NextDeadline() == start + duration, "Base selection cannot refresh HUD expiry");
    state.Tick(*state.NextDeadline());
    Require(state.Current() == IslandState::Expanded && !state.NextDeadline(),
            "Pin survives HUD and base change (DESIGN 3.0)");
    state.CloseMedia(start + duration);
    Require(state.Current() == IslandState::Dormant && !state.Pinned(), "Closing media clears pin");
    state.SetBase(IslandState::Compact, start + duration);
    state.TriggerHud(HudKind::Volume, start + duration);
    state.SetHovered(true);
    state.SetHoverExpanded(true);
    Require(state.Current() == IslandState::Hud && !state.Swell(), "Hover cannot override HUD or swell it");
    state.SetHoverExpanded(false);
    state.SetHovered(false);
    state.Tick(*state.NextDeadline());
    Require(state.Current() == IslandState::Compact && !state.NextDeadline(), "Leave during HUD must affect return state");
    state.TriggerHud(HudKind::Volume, start);
    state.SetBase(IslandState::Dormant, start + duration);
    Require(state.Current() == IslandState::Dormant && !state.NextDeadline(), "SetBase must retire already-expired HUD");
    state.PinHud(HudKind::Volume);
    state.Tick(start + duration);
    Require(state.Current() == IslandState::Hud && !state.NextDeadline(), "Key4 HUD stays pinned without timer");
    state.TriggerHud(HudKind::Brightness, start + duration);
    Require(state.NextDeadline().has_value(), "V/B/P replace pinned HUD with transient HUD");
    state.Tick(*state.NextDeadline());
    Require(state.Current() == IslandState::Dormant, "Transient HUD restores base");
    state.PinHud(HudKind::Volume);
    state.DismissHud();
    Require(state.Current() == IslandState::Dormant, "Explicit demo state dismisses pin");
    IslandSprings reduced;
    reduced.SetTarget(kGeomExpanded, Spring::ToExpanded);
    reduced.Advance(Motion::FixedStep);
    reduced.Snap();
    Require(reduced.Settled() && reduced.Value().width == kGeomExpanded.width, "Reduced motion snaps exactly");
    for (const auto& value : reduced.Values()) { Require(value.v == PaintSpec::Zero, "Snap removes velocity"); }
}

void TestHover() {
    constexpr double enter = Motion::HoverEnterMs / Motion::MillisecondsPerSecond;
    constexpr double leave = Motion::HoverLeaveMs / Motion::MillisecondsPerSecond;
    constexpr double start = Motion::MaxFrameDelta;
    HoverTracker hover;
    Require(!hover.NextDeadline() && !hover.Tick(start), "Initial hover has no event or timer");
    hover.Pointer(false, start);
    Require(!hover.NextDeadline(), "Initial outside sample must not arm collapse");
    hover.Pointer(true, start);
    const double enterDeadline = *hover.NextDeadline();
    Require(enterDeadline == start + enter, "Hover enter timing differs from spec");
    hover.Pointer(true, start + enter / 2.0);
    Require(*hover.NextDeadline() == enterDeadline, "Repeated inside samples cannot postpone expansion");
    Require(!hover.Tick(enterDeadline - Motion::FixedStep), "Hover fired before enter delay");
    Require(hover.Tick(enterDeadline) == std::optional<bool>(true), "Hover must expand at deadline");
    Require(!hover.NextDeadline() && !hover.Tick(enterDeadline), "Consumed hover timer must not repeat");
    hover.Pointer(false, enterDeadline);
    const double leaveDeadline = *hover.NextDeadline();
    Require(leaveDeadline == enterDeadline + leave, "Hover leave timing differs from spec");
    hover.Pointer(false, enterDeadline + leave / 2.0);
    Require(*hover.NextDeadline() == leaveDeadline, "Repeated outside samples cannot postpone collapse");
    Require(!hover.Tick(leaveDeadline - Motion::FixedStep), "Hover fired before leave delay");
    Require(hover.Tick(leaveDeadline) == std::optional<bool>(false), "Hover must collapse at deadline");
    Require(!hover.NextDeadline() && !hover.Tick(leaveDeadline), "Consumed leave timer must not repeat");

    HoverTracker cancelled;
    cancelled.Pointer(true, start);
    cancelled.Pointer(false, start + enter / 2.0);
    Require(!cancelled.Tick(start + enter), "Leaving must cancel pending expansion");
    const double cancelledLeave = *cancelled.NextDeadline();
    cancelled.Pointer(true, cancelledLeave - Motion::FixedStep);
    Require(!cancelled.Tick(cancelledLeave), "Reentry must cancel pending collapse");
    Require(cancelled.Tick(*cancelled.NextDeadline()) == std::optional<bool>(true), "Reentry must arm a fresh enter delay");
    Require(!cancelled.NextDeadline(), "No deadline may remain after final hover decision");

    StateMachine hovered;
    hovered.SetHoverExpanded(true);
    cancelled.Pointer(false, cancelledLeave + enter);
    const double collapseAt = *cancelled.NextDeadline();
    if (const auto decision = cancelled.Tick(collapseAt)) { hovered.SetHoverExpanded(*decision); }
    Require(hovered.Current() == IslandState::Dormant, "Delayed pointer leave must release hover expansion");
}

void TestSwellAndPin() {
    StateMachine s;
    s.SetHovered(true);
    Require(s.Current() == IslandState::Dormant && s.Swell(), "Enter swells Dormant at once");
    const auto g = TargetGeometry(IslandState::Dormant, true);
    Require(g.width == kGeomIdle.width + kSwellDeltaWidth && g.height == kGeomIdle.height + kSwellDeltaHeight &&
            g.bottomRadius == kGeomIdle.bottomRadius + kSwellDeltaBottomRadius &&
            g.earRadius == kGeomIdle.earRadius, "Swell deltas (DESIGN 3.2)");
    Require(TargetGeometry(IslandState::Expanded, true).width == kGeomExpanded.width, "No swell on Expanded");
    s.SetHoverExpanded(true);
    Require(s.Current() == IslandState::Expanded && !s.Swell(), "Hover delay expands, swell ends");
    s.SetHovered(false);
    s.SetHoverExpanded(false);
    Require(s.Current() == IslandState::Dormant, "Unpinned collapse after leave delay");

    s.SetHovered(true);
    s.Pin(); // click
    s.SetHovered(false);
    s.SetHoverExpanded(false);
    Require(s.Current() == IslandState::Expanded && !s.Swell(), "Pinned survives leave, never swells");
    s.SetHovered(true);
    s.Unpin(); // click on empty area while hovered
    Require(s.Current() == IslandState::Dormant && !s.Swell(), "Unpin collapses and suppresses hover");
    s.SetHoverExpanded(true);
    Require(s.Current() == IslandState::Dormant, "suppressHover blocks re-open");
    s.SetHovered(false);
    s.SetHovered(true);
    Require(s.Swell(), "Leaving clears suppressHover");
    s.SetHovered(false);
    s.Pin();
    s.Unpin(); // outside mouse-down: pointer outside, no suppression
    s.SetHovered(true);
    Require(s.Swell(), "Outside unpin does not suppress");
}

void TestClockDeadline() {
    auto at = [](WORD year, WORD month, WORD day, WORD hour, WORD minute, WORD second, WORD ms) {
        SYSTEMTIME t{};
        t.wYear = year; t.wMonth = month; t.wDay = day;
        t.wHour = hour; t.wMinute = minute; t.wSecond = second; t.wMilliseconds = ms;
        return MsToNextMinute(t);
    };
    Require(at(2026, 10, 23, 12, 34, 0, 0) == 60'000, "Exact boundary waits a full minute");
    Require(at(2026, 10, 23, 12, 34, 0, 1) == 59'999, "Just after boundary");
    Require(at(2026, 10, 23, 12, 34, 30, 250) == 29'750, "Mid-minute");
    Require(at(2026, 10, 23, 12, 34, 59, 999) == 1, "Minute boundary");
    Require(at(2026, 10, 23, 12, 59, 59, 500) == 500, "Hour boundary");
    Require(at(2026, 10, 23, 23, 59, 59, 999) == 1, "Day boundary");
    Require(at(2026, 12, 31, 23, 59, 59, 0) == 1'000, "Year boundary");
    Require(at(2016, 12, 31, 23, 59, 60, 0) == 1, "Leap second retries shortly");
}
} // namespace

int main() {
    try {
        TestTransitions();
        TestRetargetAndTiming();
        TestStateMachine();
        TestHover();
        TestSwellAndPin();
        TestClockDeadline();
        std::puts("PASS: spring transitions, retargeting, fixed-step timing, state priority, HUD, hover, swell, pin and clock deadline");
        return 0;
    } catch (const std::exception& error) {
        std::fprintf(stderr, "%s\n", error.what());
        return 1;
    }
}

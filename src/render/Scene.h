#pragma once
#include "state/StateMachine.h"
#include "spec/NotchIslandSpec.h"

namespace caecilia {
struct ContentLayer {
    IslandState state{IslandState::Dormant};
    HudKind hud{HudKind::Volume};
    unsigned tab{};
    float opacity{NotchIsland::PaintSpec::Unit};
    float blur{};
};
struct Scene {
    NotchIsland::IslandGeom geometry{NotchIsland::kGeomIdle};
    D2D1_COLOR_F accentColor{NotchIsland::kDefaultConfig.accentColor};
    ContentLayer incoming;
    ContentLayer outgoing;
    bool hasOutgoing{};
    bool bounds{};
    bool fpsVisible{};
    bool animating{};
    bool mediaActive{};
    bool reducedMotion{};
    float fps{};
    float tabPosition{NotchIsland::ExpandedNowPlaying::TabSel.rect.left};
    double time{};
};
} // namespace caecilia

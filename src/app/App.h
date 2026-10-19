#pragma once
#include "window/OverlayWindow.h"
#include "render/IslandGeometry.h"
#include "render/Renderer.h"
#include "render/Painter.h"
#include "render/Scene.h"
#include "data/DemoSource.h"
#include "motion/Spring.h"
#include "input/HoverTracker.h"
#include <filesystem>
#include <chrono>

namespace caecilia {
struct AppOptions {
    bool demo{};
    bool demoMediaActive{};
    bool showDebugFps{NotchIsland::DebugHud::ShowDebugFpsDefault};
    bool debugBounds{NotchIsland::DebugHud::ShowBoundsDefault};
    bool verifyDeviceRecovery{};
    std::filesystem::path config;
    NotchIsland::UserConfig userConfig{NotchIsland::kDefaultConfig};
};
AppOptions ParseOptions();

class App final : private WindowEvents {
public:
    explicit App(AppOptions options);
    ~App();
    int Run(HINSTANCE instance);
private:
    void OnDraw() override;
    void OnBoundsToggle() override;
    void OnFpsToggle() override;
    void OnResume() override;
    void OnPointer(bool inside) override;
    void OnClick(D2D1_POINT_2F point) override;
    void OnMiddleClick() override;
    void OnOutsideMouseDown() override;
    void OnDemoKey(UINT key) override;
    void OnFailure(const char* message) noexcept override;
    void Tick(double now);
    void Render(double now);
    void Transition(double now);
    [[nodiscard]] double Now() const noexcept;
    [[nodiscard]] DWORD DeadlineWait(double now) const noexcept;
    AppOptions options_;
    IslandGeometry geometry_;
    Painter painter_;
    Renderer renderer_;
    OverlayWindow window_;
    DemoSource demo_;
    StateMachine state_;
    HoverTracker hover_;
    IslandSprings springs_;
    Scene scene_;
    std::chrono::steady_clock::time_point epoch_{std::chrono::steady_clock::now()};
    double lastTick_{};
    double lastPresent_{};
    double fadeStart_{};
    double tabAccumulator_{};
    float outgoingStartOpacity_{};
    NotchIsland::SpringValue tabSpring_{NotchIsland::ExpandedNowPlaying::TabSel.rect.left,
                                      NotchIsland::PaintSpec::Zero,
                                      NotchIsland::ExpandedNowPlaying::TabSel.rect.left};
    unsigned recoveryAttempts_{};
    bool failed_{};
    bool recoveryVerified_{};
    bool dirty_{true};
    bool fading_{};
    bool swell_{};
    bool pointerInside_{};
    bool deviceUnavailable_{};
};
} // namespace caecilia

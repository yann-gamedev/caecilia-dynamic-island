#pragma once
#include "window/OverlayWindow.h"
#include "render/IslandGeometry.h"
#include "render/Renderer.h"
#include "render/Painter.h"
#include "data/DemoSource.h"
#include <filesystem>

namespace caecilia {
struct AppOptions {
    bool demo{};
    bool demoMediaActive{};
    bool showDebugFps{NotchIsland::DebugHud::ShowDebugFpsDefault};
    bool debugBounds{NotchIsland::DebugHud::ShowBoundsDefault};
    bool verifyDeviceRecovery{};
    std::filesystem::path config;
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
    void OnFailure(const char* message) noexcept override;
    AppOptions options_;
    IslandGeometry geometry_;
    Painter painter_;
    Renderer renderer_;
    OverlayWindow window_;
    DemoSource demo_;
    unsigned recoveryAttempts_{};
    bool failed_{};
    bool recoveryVerified_{};
};
} // namespace caecilia

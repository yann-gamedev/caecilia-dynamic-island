#include "app/App.h"
#include "app/Errors.h"
#include <shellapi.h>
#include <fstream>
#include <string>
#include <string_view>
#include <utility>

namespace caecilia {
namespace {
void ReadConfig(AppOptions& options) {
    if (options.config.empty()) { return; }
    std::ifstream input(options.config);
    if (!input) { throw std::runtime_error("Cannot open requested config file"); }
    std::string line;
    while (std::getline(input, line)) {
        if (!line.empty() && line.back() == '\r') { line.pop_back(); }
        if (line.empty() || line.front() == '#') { continue; }
        if (line == "showDebugFps=true") { options.showDebugFps = true; }
        else if (line == "showDebugFps=false") { options.showDebugFps = false; }
        else { throw std::runtime_error("Unknown config entry; expected showDebugFps=true or false"); }
    }
    if (input.bad()) { throw std::runtime_error("Config read failed"); }
}
}

AppOptions ParseOptions() {
    int argc{};
    LPWSTR* argv = CommandLineToArgvW(GetCommandLineW(), &argc);
    CheckWin32(argv != nullptr, "CommandLineToArgvW");
    AppOptions options;
    try {
        for (int i = 1; i < argc; ++i) {
            const std::wstring_view arg(argv[i]);
            if (arg == L"--config") {
                if (++i == argc) { throw std::runtime_error("--config requires a path"); }
                options.config = argv[i];
            }
        }
        ReadConfig(options);
        for (int i = 1; i < argc; ++i) {
            const std::wstring_view arg(argv[i]);
            if (arg == L"--demo") { options.demo = true; }
            else if (arg == L"--demo-media-active") { options.demoMediaActive = true; }
            else if (arg == L"--show-debug-fps") { options.showDebugFps = true; }
            else if (arg == L"--debug-bounds") { options.debugBounds = true; }
            else if (arg == L"--verify-device-recovery") { options.verifyDeviceRecovery = true; }
            else if (arg == L"--config") { ++i; }
            else { throw std::runtime_error("Unknown command-line argument; see README.md"); }
        }
        if (options.demoMediaActive && !options.demo) {
            throw std::runtime_error("--demo-media-active requires --demo");
        }
        LocalFree(argv);
        return options;
    } catch (...) { LocalFree(argv); throw; }
}

App::App(AppOptions options) : options_(std::move(options)),
    demo_(options_.demo && options_.demoMediaActive) {}

App::~App() {
    // DComp references must be released before the target HWND is destroyed.
    renderer_.Reset();
    window_.Close();
}

int App::Run(HINSTANCE instance) {
    geometry_.Initialize();
    painter_.Initialize();
    window_.Create(instance, geometry_, *this);
    window_.Show();
    MSG message{};
    for (;;) {
        const BOOL result = GetMessageW(&message, nullptr, 0, 0);
        if (result == -1) { CheckWin32(FALSE, "GetMessageW"); }
        if (result == 0) { break; }
        TranslateMessage(&message);
        DispatchMessageW(&message);
    }
    char count[96]{};
    std::snprintf(count, sizeof(count), "Session Present count: %llu",
                  static_cast<unsigned long long>(renderer_.PresentCount()));
    Log(count);
    return failed_ ? 1 : 0;
}

void App::OnDraw() {
    if (failed_ || window_.PixelWidth() == 0 || window_.PixelHeight() == 0) { return; }
    if (!renderer_.Render(window_.Handle(), window_.PixelWidth(), window_.PixelHeight(),
                          window_.Dpi(), geometry_, painter_, options_.debugBounds,
                          options_.showDebugFps, demo_.MediaActive())) {
        // Never busy-spin indefinitely on an unavailable GPU. After bounded
        // retries, retain the process and retry on a new invalidation (debug
        // toggle, display/resume, or WM_PAINT), not on every raw mouse packet.
        constexpr unsigned kMaxRecoveryAttempts = 3;
        if (++recoveryAttempts_ <= kMaxRecoveryAttempts) { window_.ScheduleRecovery(); }
        else { Log("GPU unavailable; automatic retries suspended until next redraw event"); }
        return;
    }
    recoveryAttempts_ = 0;
    window_.CancelRecovery();
    if (options_.verifyDeviceRecovery && !recoveryVerified_) {
        recoveryVerified_ = true;
        renderer_.Reset();
        window_.Invalidate();
        Log("Test: resource teardown requested after first Present (not a hardware device-loss injection)");
    }
    // M0 is fully settled: its final frame already says IDLE. No render timer.
}
void App::OnBoundsToggle() { options_.debugBounds = !options_.debugBounds; window_.Invalidate(); }
void App::OnFpsToggle() { options_.showDebugFps = !options_.showDebugFps; window_.Invalidate(); }
void App::OnResume() { renderer_.Reset(); recoveryAttempts_ = 0; window_.Invalidate(); }
void App::OnFailure(const char* message) noexcept {
    Log(message);
    failed_ = true;
    PostQuitMessage(1);
}
} // namespace caecilia

#include "app/App.h"
#include "app/Errors.h"
#include <shellapi.h>
#include <fstream>
#include <string>
#include <string_view>
#include <utility>
#include <cmath>
#include <iterator>
#include <algorithm>

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

namespace {
using namespace NotchIsland;
bool Hit(D2D1_RECT_F rect, D2D1_POINT_2F point) noexcept {
    return point.x >= rect.left && point.x <= rect.right && point.y >= rect.top && point.y <= rect.bottom;
}
float ClampUnit(double value) noexcept {
    return static_cast<float>(std::clamp(value, static_cast<double>(PaintSpec::Zero),
                                        static_cast<double>(PaintSpec::Unit)));
}
// Cubic evaluation coefficients are mathematical, not visual design tokens.
float Ease(float x) noexcept {
    const auto& curve = Motion::FadeBezier;
    auto cubic = [](float t, float a, float b) {
        const float u = PaintSpec::Unit - t;
        return 3 * u * u * t * a + 3 * u * t * t * b + t * t * t;
    };
    float lo = PaintSpec::Zero, hi = PaintSpec::Unit;
    for (int iteration = 0; iteration < 24; ++iteration) {
        const float mid = (lo + hi) * WindowSpec::Half;
        if (cubic(mid, curve[0], curve[2]) < x) { lo = mid; } else { hi = mid; }
    }
    return cubic((lo + hi) * WindowSpec::Half, curve[1], curve[3]);
}
}

double App::Now() const noexcept {
    return std::chrono::duration<double>(std::chrono::steady_clock::now() - epoch_).count();
}

int App::Run(HINSTANCE instance) {
    geometry_.Initialize();
    painter_.Initialize();
    scene_.mediaActive = demo_.MediaActive();
    BOOL animations = TRUE;
    CheckWin32(SystemParametersInfoW(SPI_GETCLIENTAREAANIMATION, 0, &animations, 0), "Read reduced motion");
    scene_.reducedMotion = animations == FALSE;
    window_.Create(instance, geometry_, *this);
    if (options_.demo) { window_.EnableDemoKeyboard(); }
    window_.Show();
    lastTick_ = Now();
    bool quit = false;
    while (!quit && !failed_) {
        MSG message{};
        while (PeekMessageW(&message, nullptr, 0, 0, PM_REMOVE)) {
            if (message.message == WM_QUIT) { quit = true; break; }
            TranslateMessage(&message);
            DispatchMessageW(&message);
        }
        if (quit || failed_) { break; }
        const double now = Now();
        Tick(now);
        const bool needsFrame = !deviceUnavailable_ && (dirty_ || scene_.animating);
        const HANDLE frame = renderer_.FrameReady();
        if (needsFrame && (!frame || WaitForSingleObject(frame, 0) == WAIT_OBJECT_0)) {
            Render(now);
            continue;
        }
        const DWORD count = needsFrame && frame ? 1u : 0u;
        const DWORD result = MsgWaitForMultipleObjectsEx(count, count ? &frame : nullptr,
            DeadlineWait(now), QS_ALLINPUT, MWMO_INPUTAVAILABLE);
        if (result == WAIT_FAILED) { CheckWin32(FALSE, "Wait for frame or input"); }
        if (count && result == WAIT_OBJECT_0) {
            const double ready = Now();
            Tick(ready);
            Render(ready);
        }
    }
    char count[96]{};
    std::snprintf(count, sizeof(count), "Session Present count: %llu",
                  static_cast<unsigned long long>(renderer_.PresentCount()));
    Log(count);
    return failed_ ? 1 : 0;
}

DWORD App::DeadlineWait(double now) const noexcept {
    std::optional<double> deadline = state_.NextDeadline();
    if (const auto hover = hover_.NextDeadline(); hover && (!deadline || *hover < *deadline)) { deadline = hover; }
    if (!deadline) { return INFINITE; }
    const double milliseconds = std::ceil((std::max)(0.0, *deadline - now) * Motion::MillisecondsPerSecond);
    return static_cast<DWORD>((std::min)(milliseconds, static_cast<double>(INFINITE - 1)));
}

void App::Transition(double now) {
    window_.SetOutsideClickHook(state_.Pinned()); // WH_MOUSE_LL only while pinned.
    const auto next = state_.Current();
    const bool swell = state_.Swell() && !scene_.reducedMotion; // DESIGN 8.3: no swell.
    const bool contentChanged = scene_.incoming.state != next ||
        (next == IslandState::Hud && scene_.incoming.hud != state_.Hud());
    if (!contentChanged && swell == swell_) { return; }
    const auto previous = scene_.incoming.state;
    if (contentChanged) {
        scene_.outgoing = scene_.incoming;
        outgoingStartOpacity_ = scene_.outgoing.opacity;
        scene_.hasOutgoing = true;
        scene_.incoming.state = next;
        scene_.incoming.hud = state_.Hud();
        scene_.incoming.opacity = PaintSpec::Zero;
        scene_.incoming.blur = scene_.reducedMotion ? PaintSpec::Zero : Motion::ContentBlur;
        fadeStart_ = now;
        fading_ = true;
    }
    lastTick_ = now; // Time spent idle before this event must not advance the new target.
    // SetTarget keeps position and velocity, so ToExpanded takes over mid-swell (DESIGN 3.2).
    if (previous != next) { springs_.SetTarget(TargetGeometry(next, swell), SpringFor(previous, next)); }
    else if (swell != swell_) { springs_.SetTarget(TargetGeometry(next, swell), Spring::HoverSwell); }
    swell_ = swell;
    if (scene_.reducedMotion) { springs_.Snap(); }
    dirty_ = true;
}

void App::Tick(double now) {
    const double elapsed = (std::max)(0.0, now - lastTick_);
    state_.Tick(now);
    if (const auto expanded = hover_.Tick(now)) { state_.SetHoverExpanded(*expanded); }
    Transition(now);
    const bool wasMoving = !springs_.Settled() || !IsSettled(tabSpring_) || fading_;
    if (scene_.reducedMotion) { springs_.Snap(); }
    else { springs_.Advance((std::min)(elapsed, (std::max)(0.0, now - lastTick_))); }
    lastTick_ = now;
    if (!IsSettled(tabSpring_)) {
        if (!scene_.reducedMotion) {
            tabAccumulator_ += (std::min)(elapsed, Motion::MaxFrameDelta);
            while (tabAccumulator_ >= Motion::FixedStep) {
                StepSpring(tabSpring_, Spring::ToExpanded, static_cast<float>(Motion::FixedStep));
                tabAccumulator_ -= Motion::FixedStep;
            }
        }
        if (scene_.reducedMotion || IsSettled(tabSpring_)) {
            tabSpring_.x = tabSpring_.target; tabSpring_.v = PaintSpec::Zero; tabAccumulator_ = 0.0;
        }
    }
    if (fading_) {
        const double ageMs = (now - fadeStart_) * Motion::MillisecondsPerSecond;
        const double delay = scene_.reducedMotion ? PaintSpec::Zero : Motion::FadeDelayMs;
        const double duration = scene_.reducedMotion ? Motion::ReducedFadeMs : Motion::FadeInMs;
        const double outDuration = scene_.reducedMotion ? Motion::ReducedFadeMs : Motion::FadeOutMs;
        const float progress = ClampUnit((ageMs - delay) / duration);
        scene_.incoming.opacity = progress == PaintSpec::Unit ? progress : Ease(progress);
        scene_.incoming.blur = scene_.reducedMotion ? PaintSpec::Zero : Motion::ContentBlur * (PaintSpec::Unit - scene_.incoming.opacity);
        scene_.outgoing.opacity = outgoingStartOpacity_ * (PaintSpec::Unit - ClampUnit(ageMs / outDuration));
        scene_.hasOutgoing = scene_.outgoing.opacity > PaintSpec::Zero;
        fading_ = ageMs < delay + duration || scene_.hasOutgoing;
    }
    scene_.geometry = springs_.Value();
    geometry_.Update(scene_.geometry);
    window_.UpdatePointer();
    scene_.tabPosition = tabSpring_.x;
    scene_.time = now;
    const bool wave = scene_.mediaActive && !scene_.reducedMotion &&
        (scene_.incoming.state == IslandState::Compact ||
         (scene_.hasOutgoing && scene_.outgoing.state == IslandState::Compact));
    scene_.animating = !springs_.Settled() || !IsSettled(tabSpring_) || fading_ || wave;
    if (wasMoving) { dirty_ = true; } // includes the final settled IDLE frame
}

void App::Render(double now) {
    if (failed_ || window_.PixelWidth() == 0 || window_.PixelHeight() == 0) { return; }
    scene_.bounds = options_.debugBounds;
    scene_.fpsVisible = options_.showDebugFps;
    scene_.fps = now > lastPresent_ && lastPresent_ > 0.0 ? static_cast<float>(1.0 / (now - lastPresent_)) : PaintSpec::Zero;
    dirty_ = false;
    if (!renderer_.Render(window_.Handle(), window_.PixelWidth(), window_.PixelHeight(),
                          window_.Dpi(), geometry_, painter_, scene_)) {
        deviceUnavailable_ = true;
        constexpr unsigned kMaxRecoveryAttempts = 3;
        if (++recoveryAttempts_ <= kMaxRecoveryAttempts) { window_.ScheduleRecovery(); }
        else { Log("GPU unavailable; retries suspended until a redraw event"); }
        return;
    }
    lastPresent_ = now;
    deviceUnavailable_ = false;
    recoveryAttempts_ = 0;
    window_.CancelRecovery();
    if (options_.verifyDeviceRecovery && !recoveryVerified_) {
        recoveryVerified_ = true;
        renderer_.Reset();
        dirty_ = true;
        Log("Test: resource teardown requested after first Present (not hardware device-loss injection)");
    }
}

void App::OnDraw() { dirty_ = true; deviceUnavailable_ = false; }
void App::OnBoundsToggle() { options_.debugBounds = !options_.debugBounds; OnDraw(); }
void App::OnFpsToggle() { options_.showDebugFps = !options_.showDebugFps; OnDraw(); }
void App::OnResume() {
    renderer_.Reset(); recoveryAttempts_ = 0;
    BOOL animations = TRUE;
    CheckWin32(SystemParametersInfoW(SPI_GETCLIENTAREAANIMATION, 0, &animations, 0), "Read reduced motion");
    scene_.reducedMotion = animations == FALSE;
    OnDraw();
}
void App::OnPointer(bool inside) {
    const double now = Now();
    pointerInside_ = inside;
    hover_.Pointer(inside, now);
    state_.SetHovered(inside);
    Transition(now); // Swell starts on the next frame.
}
void App::OnMiddleClick() {
    // ponytail: M1 demo toggles the wave only; SMTC TryTogglePlayPauseAsync arrives in M2.
    if (state_.Current() != IslandState::Compact || !options_.demo) { return; }
    scene_.mediaActive = !scene_.mediaActive;
    OnDraw();
}
void App::OnOutsideMouseDown() {
    if (!state_.Pinned()) { return; }
    state_.Unpin();
    Transition(Now());
}
void App::OnClick(D2D1_POINT_2F point) {
    const double now = Now();
    const auto current = state_.Current();
    if (current == IslandState::Dormant || current == IslandState::Compact) {
        state_.Pin(); Transition(now); return;
    }
    if (current != IslandState::Expanded) { return; }
    // Body point -> content point (content origin is centered, DESIGN 5.1).
    point.x -= (scene_.geometry.width - kGeomExpanded.width) * WindowSpec::Half;
    using namespace ExpandedNowPlaying;
    const D2D1_RECT_F tabs[] = {Tab0, Tab1, Tab2};
    const unsigned activeTab = scene_.incoming.tab;
    bool control = std::any_of(std::begin(tabs), std::end(tabs), [&](auto r) { return Hit(r, point); });
    if (activeTab == 0) {
        for (const auto r : {Prev.rect, Play.rect, Next.rect, ScrubHit}) { control = control || Hit(r, point); }
    } else if (activeTab == 2) {
        for (const auto& t : {ExpandedDropTray::Tile0, ExpandedDropTray::Tile1, ExpandedDropTray::Tile2}) {
            control = control || Hit(t.rect, point);
        }
    }
    if (!control) { // Empty area toggles the pin (DESIGN 9).
        if (state_.Pinned()) { state_.Unpin(); } else { state_.Pin(); }
        Transition(now);
        return;
    }
    for (unsigned tab = 0; tab < std::size(tabs); ++tab) {
        if (!Hit(tabs[tab], point) || scene_.incoming.tab == tab) { continue; }
        scene_.outgoing = scene_.incoming;
        outgoingStartOpacity_ = scene_.outgoing.opacity;
        scene_.hasOutgoing = true;
        scene_.incoming.tab = tab;
        scene_.incoming.opacity = PaintSpec::Zero;
        scene_.incoming.blur = scene_.reducedMotion ? PaintSpec::Zero : Motion::ContentBlur;
        fadeStart_ = now; lastTick_ = now; fading_ = true;
        tabSpring_.target = ExpandedNowPlaying::TabSel.rect.left + ExpandedNowPlaying::TabStride * static_cast<float>(tab);
        dirty_ = true;
        break;
    }
}
void App::OnDemoKey(UINT key) {
    if (!options_.demo || !pointerInside_) { return; }
    const double now = Now();
    switch (key) {
    case '1': case '2':
        state_.DismissHud();
        // Unpin while hovered sets suppressHover, so the chosen base stays observable.
        state_.Unpin();
        state_.SetBase(key == '1' ? IslandState::Dormant : IslandState::Compact, now);
        scene_.mediaActive = key == '2' || demo_.MediaActive();
        break;
    case '3': state_.DismissHud(); state_.Pin(); break;
    case VK_SPACE: // Fake play/pause; paused grace visuals arrive in M2.
        scene_.mediaActive = !scene_.mediaActive;
        if (scene_.mediaActive) { state_.SetBase(IslandState::Compact, now); }
        break;
    case 'C': state_.CloseMedia(now); scene_.mediaActive = false; break;
    case '4': state_.PinHud(HudKind::Volume); break;
    case 'V': state_.TriggerHud(HudKind::Volume, now); break;
    case 'B': state_.TriggerHud(HudKind::Brightness, now); break;
    case 'P': state_.TriggerHud(HudKind::Pomodoro, now); break;
    default: return;
    }
    Transition(now);
    OnDraw();
}
void App::OnFailure(const char* message) noexcept {
    Log(message);
    failed_ = true;
    PostQuitMessage(1);
}
} // namespace caecilia

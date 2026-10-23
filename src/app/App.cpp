#include "app/App.h"
#include "app/Errors.h"
#include <shellapi.h>
#include "app/Config.h"
#include "app/Clock.h"
#include <string>
#include <string_view>
#include <utility>
#include <cmath>
#include <iterator>
#include <algorithm>

namespace caecilia {
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
        const auto config = LoadConfig(options.config);
        options.userConfig = config.user;
        options.showDebugFps = config.showDebugFps;
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
    demo_(options_.demo && options_.demoMediaActive), hover_(options_.userConfig.hoverDelayMs) {}

App::~App() {
    media_.Stop();
    // DComp references must be released before the target HWND is destroyed.
    renderer_.Reset();
    window_.Close();
}

namespace {
using namespace NotchIsland;
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
bool ReducedMotion() {
    BOOL animations = TRUE;
    CheckWin32(SystemParametersInfoW(SPI_GETCLIENTAREAANIMATION, 0, &animations, 0), "Read reduced motion");
    return animations == FALSE;
}
}

double App::Now() const noexcept {
    return std::chrono::duration<double>(std::chrono::steady_clock::now() - epoch_).count();
}

int App::Run(HINSTANCE instance) {
    geometry_.Initialize();
    painter_.Initialize(options_.userConfig.titleFont);
    scene_.accentColor = options_.userConfig.accentColor;
    scene_.demo = options_.demo;
    scene_.mediaActive = demo_.MediaActive();
    scene_.reducedMotion = ReducedMotion();
    window_.Create(instance, geometry_, *this, options_.userConfig.monitorIndex);
    if (options_.demo) { window_.EnableDemoKeyboard(); }
    else { media_.Start(); }
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
        if (!options_.demo) {
            if (auto snapshot = media_.Take()) {
                scene_.media = std::move(snapshot);
                scene_.mediaActive = scene_.media->available && scene_.media->playing;
                // ponytail: M2.1 is data-only; automatic Compact/pause grace arrives in M2.2.
                dirty_ = true;
            }
        }
        Tick(now);
        const bool needsFrame = !deviceUnavailable_ && (dirty_ || scene_.animating);
        const HANDLE frame = renderer_.FrameReady();
        if (needsFrame && (!frame || WaitForSingleObject(frame, 0) == WAIT_OBJECT_0)) {
            Render(now);
            continue;
        }
        HANDLE handles[2]{};
        DWORD count = 0;
        const bool waitForFrame = needsFrame && frame;
        if (waitForFrame) { handles[count++] = frame; }
        if (!options_.demo) { handles[count++] = media_.Changed(); }
        const DWORD result = MsgWaitForMultipleObjectsEx(count, handles,
            DeadlineWait(now), QS_ALLINPUT, MWMO_INPUTAVAILABLE);
        if (result == WAIT_FAILED) { CheckWin32(FALSE, "Wait for frame, media or input"); }
        if (waitForFrame && result == WAIT_OBJECT_0) {
            const double ready = Now();
            Tick(ready);
            Render(ready);
        }
    }
    media_.Stop();
    char count[96]{};
    std::snprintf(count, sizeof(count), "Session Present count: %llu",
                  static_cast<unsigned long long>(renderer_.PresentCount()));
    Log(count);
    return failed_ ? 1 : 0;
}

DWORD App::DeadlineWait(double now) const noexcept {
    std::optional<double> deadline = state_.NextDeadline();
    if (const auto hover = hover_.NextDeadline(); hover && (!deadline || *hover < *deadline)) { deadline = hover; }
    if (clockDeadline_ && (!deadline || *clockDeadline_ < *deadline)) { deadline = clockDeadline_; }
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
    // Header clock: one wake-up per minute boundary, only while Expanded is shown.
    if (scene_.incoming.state != IslandState::Expanded) { clockDeadline_.reset(); }
    else if (!clockDeadline_ || now >= *clockDeadline_) {
        if (clockDeadline_) { dirty_ = true; }
        SYSTEMTIME local{};
        GetLocalTime(&local); // Still in the old minute after an early wake: re-arms in a few ms.
        clockDeadline_ = now + MsToNextMinute(local) / Motion::MillisecondsPerSecond;
    }
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

void App::OnDraw() { dirty_ = true; deviceUnavailable_ = false; clockDeadline_.reset(); }
void App::OnBoundsToggle() { options_.debugBounds = !options_.debugBounds; OnDraw(); }
void App::OnFpsToggle() { options_.showDebugFps = !options_.showDebugFps; OnDraw(); }
void App::OnResume() { renderer_.Reset(); recoveryAttempts_ = 0; OnDraw(); }
void App::OnSettingChange() {
    // Settings broadcasts are frequent; only a real reduced-motion flip repaints, never a GPU reset.
    const bool reduced = ReducedMotion();
    if (reduced == scene_.reducedMotion) { return; }
    scene_.reducedMotion = reduced;
    Transition(Now()); // Drops or restores Hover Swell; Tick snaps springs.
    dirty_ = true;
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
ClickTarget App::ClickTargetAt(D2D1_POINT_2F point, bool middle) const {
    return TargetAt(point, state_.Current(), scene_.incoming.tab, scene_.geometry.width, window_.Dpi(), middle);
}
void App::OnClick(ClickTarget target) {
    const double now = Now();
    switch (target) {
    case ClickTarget::MiddlePlay: OnMiddleClick(); return;
    case ClickTarget::Dormant: case ClickTarget::Compact: state_.Pin(); Transition(now); return;
    case ClickTarget::Background: // Empty area toggles the pin (DESIGN 9).
        if (state_.Pinned()) { state_.Unpin(); } else { state_.Pin(); }
        Transition(now);
        return;
    case ClickTarget::Tab0: case ClickTarget::Tab1: case ClickTarget::Tab2: break;
    default: return; // Media/scrub/file controls are presentation-only in M1.
    }
    if (state_.Current() != IslandState::Expanded) { return; }
    const unsigned tab = static_cast<unsigned>(target) - static_cast<unsigned>(ClickTarget::Tab0);
    if (scene_.incoming.tab != tab) {
        scene_.outgoing = scene_.incoming;
        outgoingStartOpacity_ = scene_.outgoing.opacity;
        scene_.hasOutgoing = true;
        scene_.incoming.tab = tab;
        scene_.incoming.opacity = PaintSpec::Zero;
        scene_.incoming.blur = scene_.reducedMotion ? PaintSpec::Zero : Motion::ContentBlur;
        fadeStart_ = now; lastTick_ = now; fading_ = true;
        tabSpring_.target = ExpandedNowPlaying::TabSel.rect.left + ExpandedNowPlaying::TabStride * static_cast<float>(tab);
        dirty_ = true;
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

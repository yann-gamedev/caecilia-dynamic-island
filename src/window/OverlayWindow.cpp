#include "window/OverlayWindow.h"
#include "render/IslandGeometry.h"
#include "app/Errors.h"
#include "spec/NotchIslandSpec.h"
#include <windowsx.h>
#include <algorithm>
#include <cassert>
#include <cmath>
#include <cwchar>
#include <exception>
#include <limits>
#include <tuple>
#include <vector>

namespace caecilia {
namespace {
constexpr wchar_t kWindowClass[] = L"Caecilia.NotchIsland.Overlay";
constexpr int kBoundsHotkey = 1;
constexpr int kFpsHotkey = 2;
constexpr UINT_PTR kRecoveryTimer = 1;
constexpr UINT_PTR kPointerTimer = 2;
constexpr UINT kRepositionMessage = WM_APP + 1;
constexpr UINT kOutsideDownMessage = WM_APP + 2;
HWND gHookTarget{}; // Single-instance app; the LL hook callback has no user data.
// Operational device-recovery backoff, not an animation or visual duration.
constexpr UINT kRecoveryBackoffMs = 1000;

struct MonitorList {
    std::vector<MONITORINFOEXW> monitors;
    std::exception_ptr error;
};

BOOL CALLBACK CollectMonitor(HMONITOR handle, HDC, LPRECT, LPARAM data) noexcept {
    auto& list = *reinterpret_cast<MonitorList*>(data);
    try {
        MONITORINFOEXW monitor{};
        monitor.cbSize = sizeof(monitor);
        CheckWin32(GetMonitorInfoW(handle, &monitor), "GetMonitorInfo");
        list.monitors.push_back(monitor);
        return TRUE;
    } catch (...) {
        list.error = std::current_exception();
        return FALSE;
    }
}

MONITORINFOEXW SelectMonitor(int requestedIndex) {
    MonitorList list;
    const BOOL enumerated = EnumDisplayMonitors(nullptr, nullptr, CollectMonitor,
                                                reinterpret_cast<LPARAM>(&list));
    if (list.error) { std::rethrow_exception(list.error); }
    CheckWin32(enumerated, "EnumDisplayMonitors");
    if (list.monitors.empty()) { throw std::runtime_error("No display monitors available"); }
    const auto order = [](const MONITORINFOEXW& a, const MONITORINFOEXW& b) {
        const auto aKey = std::tuple{!(a.dwFlags & MONITORINFOF_PRIMARY), a.rcMonitor.left, a.rcMonitor.top};
        const auto bKey = std::tuple{!(b.dwFlags & MONITORINFOF_PRIMARY), b.rcMonitor.left, b.rcMonitor.top};
        // Device name breaks ties for displays sharing the same origin.
        return aKey != bKey ? aKey < bKey : std::wcscmp(a.szDevice, b.szDevice) < 0;
    };
    std::sort(list.monitors.begin(), list.monitors.end(), order);
    const size_t index = requestedIndex >= 0 && static_cast<size_t>(requestedIndex) < list.monitors.size()
        ? static_cast<size_t>(requestedIndex) : 0;
    assert(std::is_sorted(list.monitors.begin(), list.monitors.end(), order));
    assert((list.monitors.front().dwFlags & MONITORINFOF_PRIMARY) != 0);
    assert(index < list.monitors.size());
    return list.monitors[index];
}
}

OverlayWindow::~OverlayWindow() { Close(); }

void OverlayWindow::Create(HINSTANCE instance, IslandGeometry& geometry, WindowEvents& events, int monitorIndex) {
    instance_ = instance;
    monitorIndex_ = monitorIndex;
    geometry_ = &geometry;
    events_ = &events;
    WNDCLASSEXW wc{};
    wc.cbSize = sizeof(wc);
    wc.hInstance = instance;
    wc.lpfnWndProc = Procedure;
    wc.lpszClassName = kWindowClass;
    wc.hCursor = LoadCursorW(nullptr, IDC_ARROW);
    CheckWin32(wc.hCursor != nullptr, "LoadCursor");
    CheckWin32(RegisterClassExW(&wc) != 0, "RegisterClassEx");
    constexpr DWORD styles = WS_EX_TOPMOST | WS_EX_TOOLWINDOW | WS_EX_NOACTIVATE |
        WS_EX_LAYERED | WS_EX_NOREDIRECTIONBITMAP | WS_EX_TRANSPARENT;
    hwnd_ = CreateWindowExW(styles, kWindowClass, L"Caecilia Notch Island", WS_POPUP,
        0, 0, 0, 0, nullptr, nullptr, instance, this);
    CheckWin32(hwnd_ != nullptr, "CreateWindowEx");
    // Uniform opacity initializes layered-window visibility; per-pixel alpha is
    // supplied by the premultiplied composition swap chain, not a color key.
    CheckWin32(SetLayeredWindowAttributes(hwnd_, 0, (std::numeric_limits<BYTE>::max)(), LWA_ALPHA), "SetLayeredWindowAttributes");
    Reposition();
    RAWINPUTDEVICE mouse{};
    mouse.usUsagePage = 0x01;
    mouse.usUsage = 0x02;
    mouse.dwFlags = RIDEV_INPUTSINK;
    mouse.hwndTarget = hwnd_;
    CheckWin32(RegisterRawInputDevices(&mouse, 1, sizeof(mouse)), "RegisterRawInputDevices");
    rawInput_ = true;
    boundsHotkey_ = RegisterHotKey(hwnd_, kBoundsHotkey, MOD_CONTROL | MOD_ALT | MOD_NOREPEAT, VK_F12) != FALSE;
    if (!boundsHotkey_) { Log("Ctrl+Alt+F12 unavailable; use --debug-bounds"); }
    fpsHotkey_ = RegisterHotKey(hwnd_, kFpsHotkey, MOD_CONTROL | MOD_SHIFT | MOD_NOREPEAT, 'F') != FALSE;
    if (!fpsHotkey_) { Log("Ctrl+Shift+F unavailable; use --show-debug-fps"); }
    UpdatePointer();
}

void OverlayWindow::EnableDemoKeyboard() {
    RAWINPUTDEVICE keyboard{0x01, 0x06, RIDEV_INPUTSINK, hwnd_};
    CheckWin32(RegisterRawInputDevices(&keyboard, 1, sizeof(keyboard)), "Register demo keyboard");
    keyboardInput_ = true;
}

void OverlayWindow::Show() {
    ShowWindow(hwnd_, SW_SHOWNOACTIVATE);
    Invalidate();
}

void OverlayWindow::Reposition() {
    if (positioning_) { return; }
    positioning_ = true;
    CancelClick(); // A moved/rescaled window invalidates the pressed target.
    try {
        const auto monitor = SelectMonitor(monitorIndex_);
        dpi_ = GetDpiForWindow(hwnd_);
        CheckWin32(dpi_ != 0, "GetDpiForWindow");
        using namespace NotchIsland;
        const float scale = static_cast<float>(dpi_) / WindowSpec::DpiBase;
        const float monitorWidth = static_cast<float>(monitor.rcMonitor.right - monitor.rcMonitor.left) / scale;
        const float leftDip = std::floor((monitorWidth - WindowSpec::Width) * WindowSpec::Half);
        width_ = static_cast<UINT>(std::round(WindowSpec::Width * scale));
        height_ = static_cast<UINT>(std::round(WindowSpec::Height * scale));
        const int x = monitor.rcMonitor.left + static_cast<int>(std::round(leftDip * scale));
        CheckWin32(SetWindowPos(hwnd_, HWND_TOPMOST, x, monitor.rcMonitor.top,
            static_cast<int>(width_), static_cast<int>(height_), SWP_NOACTIVATE), "SetWindowPos");
        positioning_ = false;
        if (!repositionPending_) {
            UpdatePointer();
            Invalidate();
        }
    } catch (...) {
        positioning_ = false;
        throw;
    }
}

D2D1_POINT_2F OverlayWindow::BodyPoint(POINT screen) const {
    CheckWin32(ScreenToClient(hwnd_, &screen), "ScreenToClient");
    return BodyPointFromClient({static_cast<float>(screen.x), static_cast<float>(screen.y)},
        geometry_->Value().width, static_cast<float>(dpi_));
}

void OverlayWindow::UpdatePointer() {
    if (!hwnd_ || !dpi_) { return; }
    POINT point{};
    // Secure desktop / disconnected session can temporarily deny cursor access.
    if (!GetCursorPos(&point)) { Log("GetCursorPos unavailable"); return; }
    const bool transparent = !geometry_->Contains(BodyPoint(point));
    if (transparent == transparent_) { return; }
    SetLastError(ERROR_SUCCESS);
    const LONG_PTR style = GetWindowLongPtrW(hwnd_, GWL_EXSTYLE);
    if (style == 0 && GetLastError() != ERROR_SUCCESS) { CheckWin32(FALSE, "GetWindowLongPtr"); }
    const LONG_PTR next = transparent ? style | WS_EX_TRANSPARENT : style & ~static_cast<LONG_PTR>(WS_EX_TRANSPARENT);
    SetLastError(ERROR_SUCCESS);
    const LONG_PTR previous = SetWindowLongPtrW(hwnd_, GWL_EXSTYLE, next);
    if (previous == 0 && GetLastError() != ERROR_SUCCESS) { CheckWin32(FALSE, "SetWindowLongPtr"); }
    transparent_ = transparent;
    events_->OnPointer(!transparent);
}

void OverlayWindow::Invalidate() {
    CheckWin32(InvalidateRect(hwnd_, nullptr, FALSE), "InvalidateRect");
}
void OverlayWindow::ScheduleRecovery() {
    if (!recoveryTimer_) {
        CheckWin32(SetTimer(hwnd_, kRecoveryTimer, kRecoveryBackoffMs, nullptr) != 0, "SetTimer recovery");
        recoveryTimer_ = true;
    }
}
void OverlayWindow::CancelRecovery() noexcept {
    if (recoveryTimer_) {
        if (!KillTimer(hwnd_, kRecoveryTimer)) { Log("KillTimer recovery failed"); }
        recoveryTimer_ = false;
    }
}
void OverlayWindow::SetOutsideClickHook(bool enabled) {
    if (enabled == (mouseHook_ != nullptr)) { return; }
    if (enabled) {
        gHookTarget = hwnd_;
        mouseHook_ = SetWindowsHookExW(WH_MOUSE_LL, MouseHook, instance_, 0);
        CheckWin32(mouseHook_ != nullptr, "SetWindowsHookEx WH_MOUSE_LL");
    } else {
        if (!UnhookWindowsHookEx(mouseHook_)) { Log("UnhookWindowsHookEx failed"); }
        mouseHook_ = nullptr;
    }
}

LRESULT CALLBACK OverlayWindow::MouseHook(int code, WPARAM wparam, LPARAM lparam) noexcept {
    // Only forward the point; the UI thread does the shape test. Never block the event.
    if (code == HC_ACTION && gHookTarget &&
        (wparam == WM_LBUTTONDOWN || wparam == WM_RBUTTONDOWN || wparam == WM_MBUTTONDOWN)) {
        const POINT point = reinterpret_cast<const MSLLHOOKSTRUCT*>(lparam)->pt;
        PostMessageW(gHookTarget, kOutsideDownMessage, 0, MAKELPARAM(point.x, point.y));
    }
    return CallNextHookEx(nullptr, code, wparam, lparam);
}

void OverlayWindow::CancelClick() noexcept {
    click_.Cancel();
    if (hwnd_ && GetCapture() == hwnd_ && !ReleaseCapture()) { Log("ReleaseCapture failed"); }
}

void OverlayWindow::Close() noexcept {
    CancelClick();
    if (mouseHook_) {
        if (!UnhookWindowsHookEx(mouseHook_)) { Log("UnhookWindowsHookEx failed"); }
        mouseHook_ = nullptr;
    }
    gHookTarget = nullptr;
    if (hwnd_) {
        CancelRecovery();
        if (boundsHotkey_ && !UnregisterHotKey(hwnd_, kBoundsHotkey)) { Log("UnregisterHotKey bounds failed"); }
        if (fpsHotkey_ && !UnregisterHotKey(hwnd_, kFpsHotkey)) { Log("UnregisterHotKey FPS failed"); }
        if (rawInput_) {
            RAWINPUTDEVICE mouse{0x01, 0x02, RIDEV_REMOVE, nullptr};
            if (!RegisterRawInputDevices(&mouse, 1, sizeof(mouse))) { Log("UnregisterRawInput failed"); }
            rawInput_ = false;
        }
        if (keyboardInput_) {
            RAWINPUTDEVICE keyboard{0x01, 0x06, RIDEV_REMOVE, nullptr};
            if (!RegisterRawInputDevices(&keyboard, 1, sizeof(keyboard))) { Log("Unregister demo keyboard failed"); }
            keyboardInput_ = false;
        }
        if (!DestroyWindow(hwnd_)) { Log("DestroyWindow failed"); }
        hwnd_ = nullptr;
    }
    if (instance_) {
        if (!UnregisterClassW(kWindowClass, instance_)) { Log("UnregisterClass failed"); }
        instance_ = nullptr;
    }
}

LRESULT CALLBACK OverlayWindow::Procedure(HWND hwnd, UINT message, WPARAM wparam, LPARAM lparam) noexcept {
    auto* self = reinterpret_cast<OverlayWindow*>(GetWindowLongPtrW(hwnd, GWLP_USERDATA));
    if (message == WM_NCCREATE) {
        const auto* create = reinterpret_cast<const CREATESTRUCTW*>(lparam);
        self = static_cast<OverlayWindow*>(create->lpCreateParams);
        self->hwnd_ = hwnd;
        SetLastError(ERROR_SUCCESS);
        if (SetWindowLongPtrW(hwnd, GWLP_USERDATA, reinterpret_cast<LONG_PTR>(self)) == 0 &&
            GetLastError() != ERROR_SUCCESS) { return FALSE; }
    }
    if (!self) { return DefWindowProcW(hwnd, message, wparam, lparam); }
    try { return self->HandleMessage(message, wparam, lparam); }
    catch (const std::exception& error) {
        self->events_->OnFailure(error.what());
    } catch (...) { self->events_->OnFailure("Unknown window callback failure"); }
    return message == WM_NCHITTEST ? HTTRANSPARENT : 0;
}

LRESULT OverlayWindow::HandleMessage(UINT message, WPARAM wparam, LPARAM lparam) {
    switch (message) {
    case WM_MOUSEACTIVATE: return MA_NOACTIVATE;
    case WM_ERASEBKGND: return 1;
    case WM_NCHITTEST: {
        if (!dpi_) { return HTTRANSPARENT; }
        const POINT point{GET_X_LPARAM(lparam), GET_Y_LPARAM(lparam)};
        return geometry_->Contains(BodyPoint(point)) ? HTCLIENT : HTTRANSPARENT;
    }
    case WM_INPUT: {
        // Only inspect the fixed mouse/keyboard packet. No key text is stored,
        // no logging, no suppression: keys still go to the foreground app.
        RAWINPUT packet{};
        UINT size = sizeof(packet);
        const UINT read = GetRawInputData(reinterpret_cast<HRAWINPUT>(lparam), RID_INPUT,
                                         &packet, &size, sizeof(RAWINPUTHEADER));
        if (read == static_cast<UINT>(-1)) { CheckWin32(FALSE, "GetRawInputData"); }
        if (packet.header.dwType == RIM_TYPEMOUSE) {
            UpdatePointer();
            // WM_INPUT can precede the cursor update, so GetCursorPos lags one packet.
            // Re-sample once input goes quiet; re-arming coalesces the stream.
            CheckWin32(SetTimer(hwnd_, kPointerTimer, USER_TIMER_MINIMUM, nullptr) != 0, "SetTimer pointer");
        }
        if (keyboardInput_ && packet.header.dwType == RIM_TYPEKEYBOARD) {
            const UINT key = packet.data.keyboard.VKey;
            if (key < 256) {
                const bool down = (packet.data.keyboard.Flags & RI_KEY_BREAK) == 0;
                const bool first = down && !keyDown_[key];
                keyDown_[key] = down;
                if (first && (GetAsyncKeyState(VK_CONTROL) & 0x8000) == 0 &&
                    (GetAsyncKeyState(VK_MENU) & 0x8000) == 0 &&
                    (GetAsyncKeyState(VK_SHIFT) & 0x8000) == 0) { events_->OnDemoKey(key); }
            }
        }
        return DefWindowProcW(hwnd_, message, wparam, lparam);
    }
    case WM_MOUSEMOVE:
        if (!trackingLeave_) {
            TRACKMOUSEEVENT tracking{sizeof(tracking), TME_LEAVE, hwnd_, 0};
            CheckWin32(TrackMouseEvent(&tracking), "TrackMouseEvent");
            trackingLeave_ = true;
        }
        UpdatePointer(); return 0;
    case WM_MOUSELEAVE:
        trackingLeave_ = false;
        UpdatePointer(); return 0;
    case WM_LBUTTONDOWN: case WM_MBUTTONDOWN:
    case WM_LBUTTONUP: case WM_MBUTTONUP: {
        // Client coords stay valid (possibly negative) while captured outside the window.
        const bool middle = message == WM_MBUTTONDOWN || message == WM_MBUTTONUP;
        const auto point = BodyPointFromClient({static_cast<float>(GET_X_LPARAM(lparam)),
            static_cast<float>(GET_Y_LPARAM(lparam))}, geometry_->Value().width, static_cast<float>(dpi_));
        const auto target = dpi_ && geometry_->Contains(point) ? events_->ClickTargetAt(point, middle) : ClickTarget::None;
        if (message == WM_LBUTTONDOWN || message == WM_MBUTTONDOWN) {
            click_.Down(target, middle);
            if (target != ClickTarget::None) { SetCapture(hwnd_); } else { CancelClick(); }
            return 0;
        }
        const bool fire = click_.Up(target, middle);
        CancelClick(); // Releases capture; the resulting WM_CAPTURECHANGED is a no-op.
        if (fire) { events_->OnClick(target); }
        return 0;
    }
    case WM_CAPTURECHANGED: click_.Cancel(); return 0;
    case WM_CANCELMODE: CancelClick(); return 0;
    case kOutsideDownMessage: {
        const POINT screen{GET_X_LPARAM(lparam), GET_Y_LPARAM(lparam)};
        if (mouseHook_ && !geometry_->Contains(BodyPoint(screen))) { events_->OnOutsideMouseDown(); }
        return 0;
    }
    case WM_PAINT: {
        PAINTSTRUCT paint{};
        const HDC dc = BeginPaint(hwnd_, &paint);
        if (!dc) { CheckWin32(FALSE, "BeginPaint"); }
        const BOOL ended = EndPaint(hwnd_, &paint);
        CheckWin32(ended, "EndPaint");
        events_->OnDraw();
        return 0;
    }
    case WM_DPICHANGED:
        dpi_ = HIWORD(wparam);
        if (positioning_) {
            // SetWindowPos can synchronously cross into a different-DPI selected
            // monitor. Do not discard this notification or recursively reposition.
            if (!repositionPending_) {
                CheckWin32(PostMessageW(hwnd_, kRepositionMessage, 0, 0), "Post DPI reposition");
                repositionPending_ = true;
            }
        } else { Reposition(); }
        return 0;
    case kRepositionMessage:
        repositionPending_ = false;
        Reposition(); return 0;
    case WM_DISPLAYCHANGE:
        Reposition(); return 0;
    case WM_HOTKEY:
        if (wparam == kBoundsHotkey) { events_->OnBoundsToggle(); }
        if (wparam == kFpsHotkey) { events_->OnFpsToggle(); }
        return 0;
    case WM_TIMER:
        if (wparam == kRecoveryTimer) { CancelRecovery(); Invalidate(); }
        if (wparam == kPointerTimer) { KillTimer(hwnd_, kPointerTimer); UpdatePointer(); }
        return 0;
    case WM_SETTINGCHANGE:
        events_->OnSettingChange(); return 0;
    case WM_TIMECHANGE: // Clock or time zone set: repaint and re-arm the minute deadline.
        events_->OnDraw(); return 0;
    case WM_POWERBROADCAST:
        if (wparam == PBT_APMRESUMEAUTOMATIC || wparam == PBT_APMRESUMESUSPEND) {
            Reposition(); events_->OnResume();
        }
        return TRUE;
    case WM_CLOSE: PostQuitMessage(0); return 0;
    case WM_DESTROY: PostQuitMessage(0); return 0;
    default: return DefWindowProcW(hwnd_, message, wparam, lparam);
    }
}
} // namespace caecilia

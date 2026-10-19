#pragma once
#include <windows.h>
#include <d2d1.h>

namespace caecilia {
class IslandGeometry;
struct WindowEvents {
    virtual ~WindowEvents() = default;
    virtual void OnDraw() = 0;
    virtual void OnBoundsToggle() = 0;
    virtual void OnFpsToggle() = 0;
    virtual void OnResume() = 0;
    virtual void OnPointer(bool inside) = 0;
    virtual void OnClick(D2D1_POINT_2F point) = 0;
    virtual void OnMiddleClick() = 0;
    virtual void OnOutsideMouseDown() = 0;
    virtual void OnDemoKey(UINT key) = 0;
    virtual void OnFailure(const char* message) noexcept = 0;
};

class OverlayWindow final {
public:
    ~OverlayWindow();
    void Create(HINSTANCE instance, IslandGeometry& geometry, WindowEvents& events);
    void Show();
    void EnableDemoKeyboard();
    void Reposition();
    void UpdatePointer();
    void Invalidate();
    void ScheduleRecovery();
    void CancelRecovery() noexcept;
    // DESIGN 9: WH_MOUSE_LL installed only while pinned; never blocks events.
    void SetOutsideClickHook(bool enabled);
    void Close() noexcept;
    [[nodiscard]] HWND Handle() const noexcept { return hwnd_; }
    [[nodiscard]] float Dpi() const noexcept { return static_cast<float>(dpi_); }
    [[nodiscard]] UINT PixelWidth() const noexcept { return width_; }
    [[nodiscard]] UINT PixelHeight() const noexcept { return height_; }
    [[nodiscard]] D2D1_POINT_2F BodyPoint(POINT screen) const;
private:
    static LRESULT CALLBACK Procedure(HWND hwnd, UINT message, WPARAM wparam, LPARAM lparam) noexcept;
    static LRESULT CALLBACK MouseHook(int code, WPARAM wparam, LPARAM lparam) noexcept;
    HHOOK mouseHook_{};
    LRESULT HandleMessage(UINT message, WPARAM wparam, LPARAM lparam);
    HWND hwnd_{};
    HINSTANCE instance_{};
    IslandGeometry* geometry_{};
    WindowEvents* events_{};
    UINT dpi_{};
    UINT width_{};
    UINT height_{};
    bool positioning_{};
    bool repositionPending_{};
    bool transparent_{true};
    bool rawInput_{};
    bool keyboardInput_{};
    bool trackingLeave_{};
    bool keyDown_[256]{};
    bool boundsHotkey_{};
    bool fpsHotkey_{};
    bool recoveryTimer_{};
};
} // namespace caecilia

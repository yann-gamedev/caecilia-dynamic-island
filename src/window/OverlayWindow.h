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
    virtual void OnFailure(const char* message) noexcept = 0;
};

class OverlayWindow final {
public:
    ~OverlayWindow();
    void Create(HINSTANCE instance, IslandGeometry& geometry, WindowEvents& events);
    void Show();
    void Reposition();
    void UpdatePointer();
    void Invalidate();
    void ScheduleRecovery();
    void CancelRecovery() noexcept;
    void Close() noexcept;
    [[nodiscard]] HWND Handle() const noexcept { return hwnd_; }
    [[nodiscard]] float Dpi() const noexcept { return static_cast<float>(dpi_); }
    [[nodiscard]] UINT PixelWidth() const noexcept { return width_; }
    [[nodiscard]] UINT PixelHeight() const noexcept { return height_; }
    [[nodiscard]] D2D1_POINT_2F BodyPoint(POINT screen) const;
private:
    static LRESULT CALLBACK Procedure(HWND hwnd, UINT message, WPARAM wparam, LPARAM lparam) noexcept;
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
    bool boundsHotkey_{};
    bool fpsHotkey_{};
    bool recoveryTimer_{};
};
} // namespace caecilia

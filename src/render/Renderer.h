#pragma once
#include <windows.h>
#include <d3d11.h>
#include <dxgi1_2.h>
#include <dcomp.h>
#include <d2d1_1.h>
#include <wrl/client.h>
#include <cstdint>

namespace caecilia {
class IslandGeometry;
class Painter;

class Renderer final {
public:
    ~Renderer();
    // False means a recoverable device loss; caller schedules a bounded retry.
    bool Render(HWND window, UINT width, UINT height, float dpi,
                IslandGeometry& geometry, Painter& painter,
                bool bounds, bool fps, bool mediaActive);
    void Reset() noexcept;
    [[nodiscard]] std::uint64_t PresentCount() const noexcept { return presents_; }
private:
    void Create(HWND window, UINT width, UINT height, float dpi, IslandGeometry& geometry);
    void BindTarget(float dpi);
    Microsoft::WRL::ComPtr<ID3D11Device> d3d_;
    Microsoft::WRL::ComPtr<ID3D11DeviceContext> immediate_;
    Microsoft::WRL::ComPtr<IDXGISwapChain1> swap_;
    Microsoft::WRL::ComPtr<IDCompositionDevice> composition_;
    Microsoft::WRL::ComPtr<IDCompositionTarget> compositionTarget_;
    Microsoft::WRL::ComPtr<IDCompositionVisual> visual_;
    Microsoft::WRL::ComPtr<ID2D1Device> device_;
    Microsoft::WRL::ComPtr<ID2D1DeviceContext> context_;
    Microsoft::WRL::ComPtr<ID2D1Bitmap1> target_;
    UINT width_{};
    UINT height_{};
    std::uint64_t presents_{};
    bool recovering_{};
    bool creatingDevice_{};
};
} // namespace caecilia

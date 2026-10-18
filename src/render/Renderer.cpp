#include "render/Renderer.h"
#include "render/IslandGeometry.h"
#include "render/Painter.h"
#include "app/Errors.h"

namespace caecilia {
using Microsoft::WRL::ComPtr;

Renderer::~Renderer() { Reset(); }

void Renderer::Reset() noexcept {
    if (context_) { context_->SetTarget(nullptr); }
    target_.Reset();
    context_.Reset();
    device_.Reset();
    visual_.Reset();
    compositionTarget_.Reset();
    composition_.Reset();
    swap_.Reset();
    if (immediate_) { immediate_->ClearState(); immediate_->Flush(); }
    immediate_.Reset();
    d3d_.Reset();
    width_ = height_ = 0;
}

void Renderer::BindTarget(float dpi) {
    ComPtr<IDXGISurface> surface;
    Check(swap_->GetBuffer(0, IID_PPV_ARGS(surface.GetAddressOf())), "GetBuffer");
    const auto properties = D2D1::BitmapProperties1(
        D2D1_BITMAP_OPTIONS_TARGET | D2D1_BITMAP_OPTIONS_CANNOT_DRAW,
        D2D1::PixelFormat(DXGI_FORMAT_B8G8R8A8_UNORM, D2D1_ALPHA_MODE_PREMULTIPLIED), dpi, dpi);
    Check(context_->CreateBitmapFromDxgiSurface(surface.Get(), &properties,
          target_.ReleaseAndGetAddressOf()), "CreateBitmapFromDxgiSurface");
    context_->SetTarget(target_.Get());
    context_->SetDpi(dpi, dpi);
    context_->SetTextAntialiasMode(D2D1_TEXT_ANTIALIAS_MODE_GRAYSCALE);
}

void Renderer::Create(HWND window, UINT width, UINT height, float dpi, IslandGeometry& geometry) {
    Reset();
    creatingDevice_ = true;
    HRESULT hr = D3D11CreateDevice(nullptr, D3D_DRIVER_TYPE_HARDWARE, nullptr,
        D3D11_CREATE_DEVICE_BGRA_SUPPORT, nullptr, 0, D3D11_SDK_VERSION,
        d3d_.GetAddressOf(), nullptr, immediate_.GetAddressOf());
    if (FAILED(hr)) {
        Log("Hardware D3D11 unavailable; trying WARP");
        d3d_.Reset();
        immediate_.Reset();
        hr = D3D11CreateDevice(nullptr, D3D_DRIVER_TYPE_WARP, nullptr,
            D3D11_CREATE_DEVICE_BGRA_SUPPORT, nullptr, 0, D3D11_SDK_VERSION,
            d3d_.GetAddressOf(), nullptr, immediate_.GetAddressOf());
    }
    Check(hr, "D3D11CreateDevice");
    creatingDevice_ = false;
    ComPtr<IDXGIDevice> dxgi;
    Check(d3d_.As(&dxgi), "Query IDXGIDevice");
    ComPtr<IDXGIAdapter> adapter;
    Check(dxgi->GetAdapter(adapter.GetAddressOf()), "GetAdapter");
    ComPtr<IDXGIFactory2> factory;
    Check(adapter->GetParent(IID_PPV_ARGS(factory.GetAddressOf())), "GetParent IDXGIFactory2");
    DXGI_SWAP_CHAIN_DESC1 desc{};
    desc.Width = width;
    desc.Height = height;
    desc.Format = DXGI_FORMAT_B8G8R8A8_UNORM;
    desc.SampleDesc.Count = 1;
    desc.BufferUsage = DXGI_USAGE_RENDER_TARGET_OUTPUT;
    desc.BufferCount = 2;
    desc.Scaling = DXGI_SCALING_STRETCH;
    desc.SwapEffect = DXGI_SWAP_EFFECT_FLIP_SEQUENTIAL;
    desc.AlphaMode = DXGI_ALPHA_MODE_PREMULTIPLIED;
    Check(factory->CreateSwapChainForComposition(d3d_.Get(), &desc, nullptr,
          swap_.GetAddressOf()), "CreateSwapChainForComposition");
    Check(DCompositionCreateDevice(dxgi.Get(), IID_PPV_ARGS(composition_.GetAddressOf())),
          "DCompositionCreateDevice");
    Check(composition_->CreateTargetForHwnd(window, TRUE, compositionTarget_.GetAddressOf()),
          "CreateTargetForHwnd");
    Check(composition_->CreateVisual(visual_.GetAddressOf()), "CreateVisual");
    Check(visual_->SetContent(swap_.Get()), "SetContent");
    Check(compositionTarget_->SetRoot(visual_.Get()), "SetRoot");
    Check(geometry.Factory()->CreateDevice(dxgi.Get(), device_.GetAddressOf()), "Create D2D device");
    Check(device_->CreateDeviceContext(D2D1_DEVICE_CONTEXT_OPTIONS_NONE, context_.GetAddressOf()),
          "CreateDeviceContext");
    BindTarget(dpi);
    Check(composition_->Commit(), "DirectComposition Commit");
    width_ = width;
    height_ = height;
}

bool Renderer::Render(HWND window, UINT width, UINT height, float dpi,
                      IslandGeometry& geometry, Painter& painter,
                      bool bounds, bool fps, bool mediaActive) {
    try {
        if (!swap_) { Create(window, width, height, dpi, geometry); }
        if (width != width_ || height != height_) {
            context_->SetTarget(nullptr);
            target_.Reset();
            Check(swap_->ResizeBuffers(0, width, height, DXGI_FORMAT_UNKNOWN, 0), "ResizeBuffers");
            BindTarget(dpi);
            width_ = width;
            height_ = height;
        }
        context_->SetDpi(dpi, dpi);
        painter.Draw(context_.Get(), geometry, bounds, fps, mediaActive);
        Check(swap_->Present(1, 0), "Present");
        ++presents_;
        recovering_ = false;
        return true;
    } catch (const HResultError& error) {
        const HRESULT code = error.Code();
        const bool lost = code == D2DERR_RECREATE_TARGET || code == DXGI_ERROR_DEVICE_REMOVED ||
            code == DXGI_ERROR_DEVICE_RESET || code == DXGI_ERROR_DEVICE_HUNG ||
            (d3d_ && FAILED(d3d_->GetDeviceRemovedReason()));
        const bool temporarilyUnavailable = recovering_ && creatingDevice_;
        creatingDevice_ = false;
        if (!lost && !temporarilyUnavailable) { throw; }
        recovering_ = true;
        Log(error.what());
        Reset();
        return false;
    }
}
} // namespace caecilia

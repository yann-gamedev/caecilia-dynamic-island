#include "render/Painter.h"
#include "render/IslandGeometry.h"
#include "app/Errors.h"
#include <d3d11.h>
#include <wincodec.h>
#include <cstdio>
#include <filesystem>
#include <vector>

namespace {
using namespace caecilia;
using namespace NotchIsland;
using Microsoft::WRL::ComPtr;

void Require(bool condition, const char* message) {
    if (!condition) throw std::runtime_error(message);
}

void SavePng(IWICImagingFactory* wic, const std::filesystem::path& path,
             UINT width, UINT height, float dpi, std::vector<BYTE>& pixels) {
    ComPtr<IWICBitmap> bitmap;
    Check(wic->CreateBitmapFromMemory(width, height, GUID_WICPixelFormat32bppPBGRA,
        width * 4, static_cast<UINT>(pixels.size()), pixels.data(), bitmap.GetAddressOf()), "WIC bitmap");
    ComPtr<IWICBitmapSource> straight;
    Check(WICConvertBitmapSource(GUID_WICPixelFormat32bppBGRA, bitmap.Get(), straight.GetAddressOf()),
        "Unpremultiply PNG pixels");
    ComPtr<IWICStream> stream;
    Check(wic->CreateStream(stream.GetAddressOf()), "WIC stream");
    Check(stream->InitializeFromFilename(path.c_str(), GENERIC_WRITE), "Open PNG");
    ComPtr<IWICBitmapEncoder> encoder;
    Check(wic->CreateEncoder(GUID_ContainerFormatPng, nullptr, encoder.GetAddressOf()), "PNG encoder");
    Check(encoder->Initialize(stream.Get(), WICBitmapEncoderNoCache), "Initialize PNG encoder");
    ComPtr<IWICBitmapFrameEncode> frame;
    Check(encoder->CreateNewFrame(frame.GetAddressOf(), nullptr), "PNG frame");
    Check(frame->Initialize(nullptr), "Initialize PNG frame");
    Check(frame->SetSize(width, height), "PNG size");
    Check(frame->SetResolution(dpi, dpi), "PNG DPI");
    auto format = GUID_WICPixelFormat32bppBGRA;
    Check(frame->SetPixelFormat(&format), "PNG format");
    Require(IsEqualGUID(format, GUID_WICPixelFormat32bppBGRA), "PNG must preserve alpha");
    Check(frame->WriteSource(straight.Get(), nullptr), "PNG pixels");
    Check(frame->Commit(), "Commit PNG frame");
    Check(encoder->Commit(), "Commit PNG");
}

void Run(const std::filesystem::path& root) {
    Require(root.is_absolute(), "Expected absolute output root argument");
    std::filesystem::create_directories(root);
    IslandGeometry geometry;
    geometry.Initialize();
    Painter painter;
    painter.Initialize(kDefaultConfig.titleFont);
    ComPtr<ID3D11Device> d3d;
    Check(D3D11CreateDevice(nullptr, D3D_DRIVER_TYPE_WARP, nullptr,
        D3D11_CREATE_DEVICE_BGRA_SUPPORT, nullptr, 0, D3D11_SDK_VERSION,
        d3d.GetAddressOf(), nullptr, nullptr), "Create D3D11 WARP device");
    ComPtr<IDXGIDevice> dxgi;
    Check(d3d.As(&dxgi), "Query DXGI device");
    ComPtr<ID2D1Device> device;
    Check(geometry.Factory()->CreateDevice(dxgi.Get(), device.GetAddressOf()), "Create D2D device");
    ComPtr<ID2D1DeviceContext> context;
    Check(device->CreateDeviceContext(D2D1_DEVICE_CONTEXT_OPTIONS_NONE, context.GetAddressOf()),
        "Create offscreen D2D context");
    ComPtr<IWICImagingFactory> wic;
    Check(CoCreateInstance(CLSID_WICImagingFactory, nullptr, CLSCTX_INPROC_SERVER,
        IID_PPV_ARGS(wic.GetAddressOf())), "Create WIC factory");

    for (const UINT dpi : {96u, 144u, 192u}) {
        const float resolution = static_cast<float>(dpi);
        const auto width = static_cast<UINT>(std::lround(WindowSpec::Width * resolution / WindowSpec::DpiBase));
        const auto height = static_cast<UINT>(std::lround(WindowSpec::Height * resolution / WindowSpec::DpiBase));
        Require(width == (dpi == 96 ? 492u : dpi == 144 ? 738u : 984u) &&
            height == (dpi == 96 ? 193u : dpi == 144 ? 290u : 386u),
            "Unexpected physical bitmap dimensions");
        const auto directory = root / (std::to_string(dpi) + "dpi");
        std::filesystem::create_directories(directory);
        const auto properties = D2D1::BitmapProperties1(D2D1_BITMAP_OPTIONS_TARGET,
            D2D1::PixelFormat(DXGI_FORMAT_B8G8R8A8_UNORM, D2D1_ALPHA_MODE_PREMULTIPLIED),
            resolution, resolution);
        ComPtr<ID2D1Bitmap1> target;
        Check(context->CreateBitmap(D2D1::SizeU(width, height), nullptr, 0, properties,
            target.GetAddressOf()), "Create target bitmap");
        const auto size = target->GetPixelSize();
        Require(size.width == width && size.height == height, "D2D bitmap size mismatch");
        auto readProperties = properties;
        readProperties.bitmapOptions = D2D1_BITMAP_OPTIONS_CPU_READ | D2D1_BITMAP_OPTIONS_CANNOT_DRAW;
        ComPtr<ID2D1Bitmap1> readback;
        Check(context->CreateBitmap(size, nullptr, 0, readProperties, readback.GetAddressOf()),
            "Create CPU readback bitmap");
        context->SetDpi(resolution, resolution);

        auto capture = [&](const char* name, const Scene& scene) {
            geometry.Update(scene.geometry);
            context->SetTarget(target.Get());
            painter.Draw(context.Get(), geometry, scene);
            context->SetTarget(nullptr);
            Check(readback->CopyFromBitmap(nullptr, target.Get(), nullptr), "Read back render");
            std::vector<BYTE> pixels(static_cast<std::size_t>(width) * height * 4);
            D2D1_MAPPED_RECT mapped{};
            Check(readback->Map(D2D1_MAP_OPTIONS_READ, &mapped), "Map render");
            for (UINT y = 0; y < height; ++y) {
                std::copy_n(mapped.bits + static_cast<std::size_t>(y) * mapped.pitch, width * 4,
                    pixels.data() + static_cast<std::size_t>(y) * width * 4);
            }
            Check(readback->Unmap(), "Unmap render");
            const auto alpha = [&](UINT x, UINT y) { return pixels[(static_cast<std::size_t>(y) * width + x) * 4 + 3]; };
            // Shadow may enter the envelope; it must not become an opaque rectangular clear.
            Require(alpha(0, height - 1) < 32 && alpha(width - 1, height - 1) < 32 &&
                alpha(0, 0) < 32 && alpha(width - 1, 0) < 32, "Opaque transparent envelope");
            const float surfaceAlpha = Color::Core.a + (Color::Obsidian.a - Color::Core.a) * scene.geometry.surfaceMix;
            Require(alpha(width / 2, static_cast<UINT>(scene.geometry.height * resolution /
                WindowSpec::DpiBase / 2)) >= static_cast<BYTE>(surfaceAlpha * 255), "Missing island body");
            if (scene.incoming.state == IslandState::Dormant && !scene.bounds) {
                Require(alpha(0, 0) == 0 && alpha(width - 1, 0) == 0 &&
                    alpha(width / 2, height - 1) == 0, "Dormant envelope must be fully transparent");
            }
            const auto path = directory / (std::string(name) + ".png");
            SavePng(wic.Get(), path, width, height, resolution, pixels);
            // Check the persisted evidence, not only the requested encoder size.
            ComPtr<IWICBitmapDecoder> decoder;
            Check(wic->CreateDecoderFromFilename(path.c_str(), nullptr, GENERIC_READ,
                WICDecodeMetadataCacheOnLoad, decoder.GetAddressOf()), "Reopen PNG");
            ComPtr<IWICBitmapFrameDecode> frame;
            Check(decoder->GetFrame(0, frame.GetAddressOf()), "Decode PNG frame");
            UINT savedWidth{}, savedHeight{};
            Check(frame->GetSize(&savedWidth, &savedHeight), "Read PNG dimensions");
            Require(savedWidth == width && savedHeight == height, "Saved PNG dimensions differ");
            std::printf("%ux%u: %s\n", width, height, path.string().c_str());
            return pixels;
        };
        Scene scene;
        scene.mediaActive = true;
        scene.time = 0.1;
        // Settled scenes, no orchestration, outgoing layer, spring or FPS overlay.
        capture("dormant", scene);
        scene.incoming.state = IslandState::Compact;
        scene.geometry = GeometryFor(scene.incoming.state);
        const auto normalEarly = capture("compact-normal-0.1", scene);
        scene.time = 0.6;
        const auto normalLate = capture("compact-normal-0.6", scene);
        Require(normalEarly != normalLate, "Active normal Compact must animate");
        scene.reducedMotion = true;
        scene.time = 0.1;
        const auto reducedEarly = capture("compact-reduced-0.1", scene);
        scene.time = 0.6;
        const auto reducedLate = capture("compact-reduced-0.6", scene);
        Require(reducedEarly == reducedLate, "Reduced-motion Compact must be pixel-identical");
        scene.reducedMotion = false;
        scene.incoming.state = IslandState::Expanded;
        scene.geometry = GeometryFor(scene.incoming.state);
        const float tabs[]{ExpandedNowPlaying::TabSel.rect.left, ExpandedTelemetry::TabSel.rect.left,
            ExpandedDropTray::TabSel.rect.left};
        const char* names[]{"expanded-now-playing", "expanded-telemetry", "expanded-drop-tray"};
        for (unsigned tab = 0; tab < 3; ++tab) {
            scene.incoming.tab = tab;
            scene.tabPosition = tabs[tab];
            capture(names[tab], scene);
            scene.bounds = true;
            capture((std::string(names[tab]) + "-bounds").c_str(), scene);
            scene.bounds = false;
        }
        scene.demo = false;
        scene.incoming.tab = 0;
        scene.tabPosition = tabs[0];
        const auto emptyMedia = capture("media-empty", scene);
        auto media = std::make_shared<MediaSnapshot>();
        media->available = true;
        media->title = L"Live metadata \u2014 \u65e5\u672c\u8a9e";
        media->artist = L"Test artist";
        media->durationSeconds = 240;
        media->positionSeconds = 60;
        auto art = std::make_shared<MediaArtwork>();
        art->width = art->height = 2;
        art->pixels = {0, 0, 255, 255, 0, 0, 255, 255, 0, 0, 255, 255, 0, 0, 255, 255};
        media->artwork = art;
        scene.media = media;
        const auto liveMedia = capture("media-live", scene);
        Require(liveMedia != emptyMedia, "Live metadata/artwork must replace the empty view");
        const auto origin = BodyOrigin(scene.geometry.width, resolution, resolution);
        const auto artX = static_cast<UINT>((origin.x + ExpandedNowPlaying::Art.rect.left + 32) * resolution / 96);
        const auto artY = static_cast<UINT>(78 * resolution / 96);
        const auto pixel = (static_cast<std::size_t>(artY) * width + artX) * 4;
        Require(liveMedia[pixel + 2] == 255 && liveMedia[pixel + 1] == 0 && liveMedia[pixel] == 0,
            "Decoded BGRA artwork must render in the album rect");
        scene.media.reset();
        const auto clearedMedia = capture("media-cleared", scene);
        Require(clearedMedia[pixel + 1] != 0, "Session removal must clear cached artwork");
        scene.demo = true;
        scene.incoming.state = IslandState::Hud;
        scene.geometry = GeometryFor(scene.incoming.state);
        const HudKind kinds[]{HudKind::Volume, HudKind::Brightness, HudKind::Pomodoro};
        const char* hudNames[]{"hud-volume", "hud-brightness", "hud-pomodoro"};
        for (unsigned i = 0; i < 3; ++i) {
            scene.incoming.hud = kinds[i];
            capture(hudNames[i], scene);
        }
    }
    // ponytail: production Expanded reads the wall clock; inject a clock only if golden-image comparisons are added.
    std::puts("PASS: 51 WARP/D2D PNGs, dimensions, alpha envelope, normal/reduced Compact, media artwork and clearing.\n"
        "Offscreen SetDpi coverage only: NOT GetDpiForWindow/WM_DPICHANGED or desktop composition.");
}
}

int wmain(int argc, wchar_t** argv) {
    bool initialized = false;
    try {
        Require(argc == 2, "Usage: CaeciliaRenderTests <absolute-output-root>");
        Check(CoInitializeEx(nullptr, COINIT_MULTITHREADED), "Initialize COM");
        initialized = true;
        Run(argv[1]);
        CoUninitialize();
        return 0;
    } catch (const std::exception& error) {
        if (initialized) CoUninitialize();
        std::fprintf(stderr, "%s\n", error.what());
        return 1;
    }
}

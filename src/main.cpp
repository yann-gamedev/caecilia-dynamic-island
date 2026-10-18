#include "app/App.h"
#include "app/Errors.h"
#include <objbase.h>
#include <exception>

namespace {
class UniqueHandle final {
public:
    explicit UniqueHandle(HANDLE handle) noexcept : handle_(handle) {}
    ~UniqueHandle() { if (handle_ && !CloseHandle(handle_)) { caecilia::Log("CloseHandle failed"); } }
    UniqueHandle(const UniqueHandle&) = delete;
    UniqueHandle& operator=(const UniqueHandle&) = delete;
private:
    HANDLE handle_;
};
class Apartment final {
public:
    Apartment() { caecilia::Check(CoInitializeEx(nullptr, COINIT_APARTMENTTHREADED), "CoInitializeEx"); }
    ~Apartment() { CoUninitialize(); }
    Apartment(const Apartment&) = delete;
    Apartment& operator=(const Apartment&) = delete;
};
}

int WINAPI wWinMain(HINSTANCE instance, HINSTANCE, PWSTR, int) {
    try {
        caecilia::CheckWin32(SetProcessDpiAwarenessContext(DPI_AWARENESS_CONTEXT_PER_MONITOR_AWARE_V2),
                            "SetProcessDpiAwarenessContext");
        const HANDLE mutex = CreateMutexW(nullptr, FALSE, L"Local\\Caecilia.NotchIsland.SingleInstance");
        const DWORD mutexError = GetLastError();
        caecilia::CheckWin32(mutex != nullptr, "CreateMutexW");
        const UniqueHandle mutexOwner(mutex);
        if (mutexError == ERROR_ALREADY_EXISTS) { return 0; }
        const Apartment apartment;
        caecilia::App app(caecilia::ParseOptions());
        return app.Run(instance);
    } catch (const std::exception& error) {
        caecilia::Log(error.what());
        // Do not open a focus-stealing error dialog from a no-activate overlay.
        return 1;
    } catch (...) {
        caecilia::Log("Unhandled startup failure");
        return 1;
    }
}

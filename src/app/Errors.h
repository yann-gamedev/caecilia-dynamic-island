#pragma once

#include <windows.h>
#include <stdexcept>
#include <string>
#include <cstdio>

namespace caecilia {

class HResultError final : public std::runtime_error {
public:
    HResultError(HRESULT code, const char* operation)
        : std::runtime_error(Message(code, operation)), code_(code) {}
    [[nodiscard]] HRESULT Code() const noexcept { return code_; }
private:
    static std::string Message(HRESULT code, const char* operation) {
        char text[32]{};
        std::snprintf(text, sizeof(text), " (HRESULT 0x%08lX)",
                      static_cast<unsigned long>(code));
        return std::string(operation) + text;
    }
    HRESULT code_;
};

inline void Check(HRESULT result, const char* operation) {
    if (FAILED(result)) { throw HResultError(result, operation); }
}

inline void CheckWin32(BOOL success, const char* operation) {
    if (!success) {
        const DWORD error = GetLastError();
        Check(HRESULT_FROM_WIN32(error == ERROR_SUCCESS ? ERROR_GEN_FAILURE : error), operation);
    }
}

inline void Log(const char* message) noexcept {
    OutputDebugStringA("Caecilia: ");
    OutputDebugStringA(message);
    OutputDebugStringA("\n");
}

} // namespace caecilia

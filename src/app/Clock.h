#pragma once
#include <windows.h>

namespace caecilia {
// Every time zone offset is whole minutes, so only seconds/ms decide the next local minute.
// Exactly on a boundary the new minute is already painted: wait a full minute.
constexpr DWORD MsToNextMinute(const SYSTEMTIME& local) noexcept {
    const DWORD into = local.wSecond * 1000u + local.wMilliseconds;
    return into < 60'000u ? 60'000u - into : 1u; // wSecond == 60 (leap second): retry shortly.
}
} // namespace caecilia

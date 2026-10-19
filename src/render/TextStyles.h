#pragma once

#include "../spec/NotchIslandSpec.h"

#include <dwrite_1.h>
#include <wrl/client.h>

#include <array>
#include <deque>
#include <string>
#include <string_view>

namespace caecilia {

class TextStyles {
public:
    void Initialize();
    Microsoft::WRL::ComPtr<IDWriteTextLayout> IdleLayout();
    Microsoft::WRL::ComPtr<IDWriteTextLayout> Layout(std::wstring_view text,
        NotchIsland::Typography::Role role, D2D1_RECT_F rect,
        DWRITE_TEXT_ALIGNMENT alignment = DWRITE_TEXT_ALIGNMENT_LEADING,
        DWRITE_PARAGRAPH_ALIGNMENT vertical = DWRITE_PARAGRAPH_ALIGNMENT_NEAR,
        bool forceTabular = false);

private:
    Microsoft::WRL::ComPtr<IDWriteFactory> factory_;
    std::array<Microsoft::WRL::ComPtr<IDWriteTextFormat>,
        static_cast<std::size_t>(NotchIsland::Typography::Role::Count)> formats_;
    struct CachedLayout {
        std::wstring text;
        NotchIsland::Typography::Role role;
        float width, height;
        DWRITE_TEXT_ALIGNMENT alignment;
        DWRITE_PARAGRAPH_ALIGNMENT vertical;
        bool tabular;
        Microsoft::WRL::ComPtr<IDWriteTextLayout> layout;
    };
    // Resource limit, not a visual measurement. FIFO keeps changing FPS bounded.
    static constexpr std::size_t CacheCapacity = 128;
    std::deque<CachedLayout> layouts_;
};

} // namespace caecilia

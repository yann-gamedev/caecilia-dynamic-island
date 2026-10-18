#pragma once

#include "../spec/NotchIslandSpec.h"

#include <dwrite_1.h>
#include <wrl/client.h>

#include <array>

namespace caecilia {

class TextStyles {
public:
    void Initialize();
    Microsoft::WRL::ComPtr<IDWriteTextLayout> IdleLayout();

private:
    Microsoft::WRL::ComPtr<IDWriteFactory> factory_;
    std::array<Microsoft::WRL::ComPtr<IDWriteTextFormat>,
        static_cast<std::size_t>(NotchIsland::Typography::Role::Count)> formats_;
    Microsoft::WRL::ComPtr<IDWriteTextLayout> idleLayout_;
};

} // namespace caecilia

#pragma once
#include <d2d1_1.h>
#include <wrl/client.h>
#include "spec/NotchIslandSpec.h"

namespace caecilia {
class IslandGeometry final {
public:
    void Initialize();
    [[nodiscard]] ID2D1Factory1* Factory() const noexcept { return factory_.Get(); }
    [[nodiscard]] ID2D1PathGeometry* Path() const noexcept { return path_.Get(); }
    [[nodiscard]] bool Contains(D2D1_POINT_2F point) const;
private:
    Microsoft::WRL::ComPtr<ID2D1Factory1> factory_;
    Microsoft::WRL::ComPtr<ID2D1PathGeometry> path_;
};
} // namespace caecilia

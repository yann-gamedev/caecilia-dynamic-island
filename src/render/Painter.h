#pragma once
#include <d2d1_1.h>
#include "render/TextStyles.h"
#include "render/Scene.h"

namespace caecilia {
class IslandGeometry;
class Painter final {
public:
    void Initialize();
    void Draw(ID2D1DeviceContext* context, IslandGeometry& geometry, const Scene& scene);
private:
    TextStyles text_;
    Microsoft::WRL::ComPtr<ID2D1Device> shadowDevice_;
    Microsoft::WRL::ComPtr<ID2D1Effect> shadow_;
    NotchIsland::IslandGeom shadowGeometry_{};
    bool shadowAnimating_{};
};
} // namespace caecilia

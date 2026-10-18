#pragma once
#include <d2d1_1.h>
#include "render/TextStyles.h"

namespace caecilia {
class IslandGeometry;
class Painter final {
public:
    void Initialize();
    void Draw(ID2D1DeviceContext* context, IslandGeometry& geometry,
              bool bounds, bool fps, bool mediaActive);
private:
    TextStyles text_;
};
} // namespace caecilia

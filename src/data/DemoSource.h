#pragma once

namespace caecilia {
// M1 uses deterministic spec samples and App-managed state/animation timing.
// No media, telemetry, audio, brightness, or network APIs are read here.
class DemoSource final {
public:
    explicit DemoSource(bool mediaActive = false) noexcept : mediaActive_(mediaActive) {}
    [[nodiscard]] bool MediaActive() const noexcept { return mediaActive_; }
private:
    bool mediaActive_{};
};
} // namespace caecilia

#pragma once

namespace caecilia {
// M0 intentionally has no timeline, state transitions, or background polling.
// It supplies a reproducible static Dormant sample only.
class DemoSource final {
public:
    explicit DemoSource(bool mediaActive = false) noexcept : mediaActive_(mediaActive) {}
    [[nodiscard]] bool MediaActive() const noexcept { return mediaActive_; }
private:
    bool mediaActive_{};
};
} // namespace caecilia

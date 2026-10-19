#pragma once

#include <optional>

namespace caecilia {
// Timing only: the window supplies shape hit tests and native mouse-leave events.
class HoverTracker {
public:
    // now is caller-provided steady_clock seconds. Repeated samples do not rearm.
    void Pointer(bool inside, double now) noexcept;
    std::optional<bool> Tick(double now) noexcept;
    std::optional<double> NextDeadline() const noexcept { return deadline_; }

private:
    bool inside_ = false;
    std::optional<double> deadline_;
};
} // namespace caecilia

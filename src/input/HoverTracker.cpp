#include "input/HoverTracker.h"
#include "spec/NotchIslandSpec.h"

namespace caecilia {
void HoverTracker::Pointer(bool inside, double now) noexcept {
    if (inside == inside_) { return; }
    inside_ = inside;
    const auto delay = inside ? enterDelayMs_ : NotchIsland::Motion::HoverLeaveMs;
    // One deadline represents either enter or leave, so the opposite timer is cancelled.
    deadline_ = now + delay / NotchIsland::Motion::MillisecondsPerSecond;
}

std::optional<bool> HoverTracker::Tick(double now) noexcept {
    if (!deadline_ || now < *deadline_) { return std::nullopt; }
    deadline_.reset();
    return inside_;
}
} // namespace caecilia

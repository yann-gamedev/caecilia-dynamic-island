#include "data/MediaSource.h"
#include <cstdio>
#include <exception>
#include <stdexcept>

namespace {
using namespace caecilia;
using namespace caecilia::media_detail;
void Require(bool condition, const char* message) {
    if (!condition) { throw std::runtime_error(message); }
}
} // namespace

int main() {
    try {
        const Candidate sessions[]{{3, L"b.app", false}, {1, L"a.app", false}, {2, L"c.app", true}};
        Require(Select({}, 0, 0) == 0, "No sessions selects nothing");
        Require(Select(sessions, 1, 0) == 1, "Windows current session wins");
        Require(Select(sessions, 0, 0) == 2, "Playing beats paused");
        Require(Select(sessions, 99, 0) == 2, "Unknown current falls back");
        const Candidate paused[]{{3, L"b.app", false}, {1, L"a.app", false}};
        Require(Select(paused, 0, 0) == 1, "Stable source order");
        const Candidate twins[]{{4, L"a.app", true}, {5, L"a.app", true}};
        Require(Select(twins, 0, 0) == 4, "Same app ties use lowest identity");
        Require(Select(twins, 0, 5) == 5, "Same app ties retain selected identity");
        const Candidate reordered[]{{5, L"a.app", true}, {4, L"a.app", true}};
        Require(Select(reordered, 0, 0) == 4 && Select(reordered, 0, 5) == 5, "Tie is order-independent");

        MediaSource source;
        source.Stop(); // Never started: publishes an empty snapshot without a worker.
        const auto cleared = source.Take();
        Require(cleared && !cleared->available, "Stop publishes an empty snapshot");
        Require(WaitForSingleObject(source.Changed(), 0) == WAIT_TIMEOUT, "Take resets Changed");
        Require(!source.Take(), "Take consumes the latest snapshot");

        Require(Current(2, 7, 2, 7), "Matching identity/revision is current");
        Require(!Current(2, 6, 2, 7), "Older same-session revision is stale");
        Require(!Current(1, 7, 2, 7), "Other session is stale");

        Require(!EncodedSize(0) && EncodedSize(8ULL << 20) && !EncodedSize((8ULL << 20) + 1),
            "Encoded artwork bound");
        Require(ImageSize(8192, 4096) && !ImageSize(0, 1) && !ImageSize(8193, 1), "Decode bound");

        MediaSnapshot snapshot;
        Timeline(snapshot, 0, 2'400'000'000, 600'000'000);
        Require(snapshot.durationSeconds == 240 && snapshot.positionSeconds == 60, "Valid timeline");
        Timeline(snapshot, 0, 0, 50);
        Require(snapshot.durationSeconds == 0 && snapshot.positionSeconds == 0, "Incoherent timeline clears");
        Timeline(snapshot, -1, 10, 5);
        Require(snapshot.durationSeconds == 0, "Negative start clears");
        std::puts("PASS: media selection, stale revision, artwork bounds and timeline validation.");
        return 0;
    } catch (const std::exception& error) {
        std::fprintf(stderr, "FAIL: %s\n", error.what());
        return 1;
    }
}

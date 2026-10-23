#pragma once
#include <cstdint>
#include <memory>
#include <string>
#include <vector>

namespace caecilia {
struct MediaArtwork {
    unsigned width{}, height{};
    std::vector<std::uint8_t> pixels; // Premultiplied BGRA, tightly packed; decoded off the UI thread.
};
struct MediaSnapshot {
    std::uint64_t sessionId{}, revision{}; // Identity + refresh generation, not an application name.
    bool available{}, playing{};
    std::wstring source, title, artist;
    double positionSeconds{}, durationSeconds{}; // Validated event snapshot; no extrapolation in M2.1.
    std::shared_ptr<const MediaArtwork> artwork;
};
} // namespace caecilia

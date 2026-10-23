#pragma once
#include "data/MediaSnapshot.h"
#include <windows.h>
#include <span>
#include <string_view>

namespace caecilia {
class MediaSource final {
public:
    MediaSource();
    ~MediaSource();
    MediaSource(const MediaSource&) = delete;
    MediaSource& operator=(const MediaSource&) = delete;
    void Start();
    void Stop() noexcept;
    // Manual-reset event. Take consumes the latest value and resets it atomically.
    HANDLE Changed() const noexcept;
    std::shared_ptr<const MediaSnapshot> Take();
private:
    struct Impl;
    std::unique_ptr<Impl> impl_;
};

// Shared, platform-independent decisions, exposed for the small native test.
namespace media_detail {
struct Candidate { std::uint64_t id; std::wstring_view source; bool playing; };
std::uint64_t Select(std::span<const Candidate> sessions, std::uint64_t current,
                     std::uint64_t retained) noexcept;
bool Current(std::uint64_t id, std::uint64_t revision,
             std::uint64_t selected, std::uint64_t latestRevision) noexcept;
bool EncodedSize(std::uint64_t bytes) noexcept;
bool ImageSize(unsigned width, unsigned height) noexcept;
void Timeline(MediaSnapshot& snapshot, std::int64_t start, std::int64_t end,
              std::int64_t position) noexcept;
} // namespace media_detail
} // namespace caecilia

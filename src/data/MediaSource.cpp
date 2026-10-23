#include "data/MediaSource.h"
#include "app/Errors.h"
#include <unknwn.h>
#include <wincodec.h>
#include <winrt/Windows.Foundation.h>
#include <winrt/Windows.Foundation.Collections.h>
#include <winrt/Windows.Media.Control.h>
#include <winrt/Windows.Storage.Streams.h>
#include <algorithm>
#include <cmath>
#include <mutex>
#include <optional>
#include <thread>
#include <utility>

namespace caecilia {
namespace media_detail {
std::uint64_t Select(std::span<const Candidate> sessions, std::uint64_t current,
                     std::uint64_t retained) noexcept {
    // Order: manager current (if listed) > playing > source ID > retained identity > creation order.
    const Candidate* best = nullptr;
    for (const auto& item : sessions) {
        if (current != 0 && item.id == current) return current;
        if (!best || (item.playing && !best->playing) ||
            (item.playing == best->playing && (item.source < best->source ||
             (item.source == best->source && (item.id == retained ||
              (best->id != retained && item.id < best->id)))))) best = &item;
    }
    return best ? best->id : 0;
}
bool Current(std::uint64_t id, std::uint64_t revision,
             std::uint64_t selected, std::uint64_t latestRevision) noexcept {
    return id == selected && revision == latestRevision;
}
bool EncodedSize(std::uint64_t bytes) noexcept { return bytes > 0 && bytes <= 8ULL * 1024 * 1024; }
bool ImageSize(unsigned width, unsigned height) noexcept {
    return width > 0 && height > 0 && width <= 8192 && height <= 8192 &&
        static_cast<std::uint64_t>(width) * height <= 32ULL * 1024 * 1024;
}
void Timeline(MediaSnapshot& snapshot, std::int64_t start, std::int64_t end,
              std::int64_t position) noexcept {
    snapshot.positionSeconds = snapshot.durationSeconds = 0;
    if (start < 0 || end < start || position < start || position > end) return;
    // No overflow: all values are validated nonnegative and ordered.
    const double duration = static_cast<double>(end - start) / 10000000.0;
    const double elapsed = static_cast<double>(position - start) / 10000000.0;
    if (std::isfinite(duration) && std::isfinite(elapsed)) {
        snapshot.durationSeconds = duration;
        snapshot.positionSeconds = elapsed;
    }
}
} // namespace media_detail

namespace {
using namespace winrt::Windows::Foundation;
using namespace winrt::Windows::Media::Control;
using namespace winrt::Windows::Storage::Streams;
using Manager = GlobalSystemMediaTransportControlsSessionManager;
using Session = GlobalSystemMediaTransportControlsSession;
using Properties = GlobalSystemMediaTransportControlsSessionMediaProperties;
using PlaybackStatus = GlobalSystemMediaTransportControlsSessionPlaybackStatus;
constexpr unsigned FlagEnumerate = 1, FlagSelection = 2, FlagMetadata = 4, FlagPlayback = 8, FlagTimeline = 16;

void LogFailure() noexcept {
    try { throw; }
    catch (const winrt::hresult_error& error) {
        Log("GSMTC operation failed");
        OutputDebugStringW(error.message().c_str());
        OutputDebugStringW(L"\n");
    }
    catch (const std::exception& error) { Log(error.what()); }
    catch (...) { Log("GSMTC operation failed (unknown exception)"); }
}

struct Event {
    HANDLE value{};
    explicit Event(bool manual) : value(CreateEventW(nullptr, manual ? TRUE : FALSE, FALSE, nullptr)) {
        CheckWin32(value != nullptr, "CreateEvent(media)");
    }
    ~Event() { CloseHandle(value); }
    Event(const Event&) = delete;
    Event& operator=(const Event&) = delete;
};

// Outlives the worker: WinRT callbacks only hold weak references to this.
struct Shared {
    Event wake{false}, changed{true};
    std::mutex mutex;
    bool stopping{};
    unsigned dirty{};
    std::uint64_t selected{}, revision{};
    std::shared_ptr<const MediaSnapshot> latest, last; // latest: untaken; last: last published.

    // Publishes an immediate clear for a new revision so old track data never lingers.
    void ClearLocked(bool keepSource) {
        auto empty = std::make_shared<MediaSnapshot>();
        if (keepSource && last && last->sessionId == selected) {
            empty->available = last->available;
            empty->playing = last->playing;
            empty->source = last->source;
        }
        empty->sessionId = selected;
        empty->revision = ++revision;
        latest = last = std::move(empty);
        SetEvent(changed.value);
    }
    void Mark(unsigned flags, std::uint64_t sender) noexcept {
        std::lock_guard lock(mutex);
        if (stopping || (sender && sender != selected)) return;
        dirty |= flags;
        if (flags & FlagMetadata) {
            try { ClearLocked(true); } catch (...) { ++revision; latest.reset(); } // Still invalidates in flight.
        }
        SetEvent(wake.value);
    }
};

auto Notify(const std::shared_ptr<Shared>& shared, unsigned flags, std::uint64_t sender = 0) {
    return [weak = std::weak_ptr<Shared>(shared), flags, sender](const auto&, const auto&) noexcept {
        if (auto state = weak.lock()) state->Mark(flags, sender);
    };
}
template<class Operation>
void OnCompletion(const Operation& operation, const std::shared_ptr<Shared>& shared) {
    operation.Completed([weak = std::weak_ptr<Shared>(shared)](const auto&, AsyncStatus) noexcept {
        if (auto state = weak.lock()) SetEvent(state->wake.value);
    });
}
template<class Operation>
void Cancel(Operation& operation) noexcept {
    if (operation) {
        try { operation.Cancel(); } catch (...) { LogFailure(); }
        operation = nullptr;
    }
}
template<class Operation>
bool Ready(const Operation& operation) { return operation && operation.Status() != AsyncStatus::Started; }
bool IsPlaying(const Session& session) noexcept {
    try {
        const auto info = session.GetPlaybackInfo();
        return info && info.PlaybackStatus() == PlaybackStatus::Playing;
    } catch (...) { LogFailure(); }
    return false;
}

std::shared_ptr<const MediaArtwork> Decode(const std::vector<std::uint8_t>& bytes) {
    if (!media_detail::EncodedSize(bytes.size())) return {};
    auto factory = winrt::create_instance<IWICImagingFactory>(CLSID_WICImagingFactory);
    winrt::com_ptr<IWICStream> stream;
    winrt::check_hresult(factory->CreateStream(stream.put()));
    // WIC's signature is non-const; the decoder only reads this buffer.
    winrt::check_hresult(stream->InitializeFromMemory(const_cast<BYTE*>(bytes.data()),
                                                     static_cast<DWORD>(bytes.size())));
    winrt::com_ptr<IWICBitmapDecoder> decoder;
    winrt::check_hresult(factory->CreateDecoderFromStream(stream.get(), nullptr,
        WICDecodeMetadataCacheOnDemand, decoder.put()));
    winrt::com_ptr<IWICBitmapFrameDecode> frame;
    winrt::check_hresult(decoder->GetFrame(0, frame.put()));
    UINT width{}, height{};
    winrt::check_hresult(frame->GetSize(&width, &height));
    if (!media_detail::ImageSize(width, height)) {
        Log("GSMTC artwork rejected: dimensions exceed decode bounds");
        return {};
    }
    const double scale = std::min(1.0, 256.0 / static_cast<double>(std::max(width, height)));
    const UINT outWidth = std::clamp(static_cast<UINT>(static_cast<double>(width) * scale), 1U, 256U);
    const UINT outHeight = std::clamp(static_cast<UINT>(static_cast<double>(height) * scale), 1U, 256U);
    auto source = frame.as<IWICBitmapSource>();
    if (width != outWidth || height != outHeight) {
        winrt::com_ptr<IWICBitmapScaler> scaler;
        winrt::check_hresult(factory->CreateBitmapScaler(scaler.put()));
        winrt::check_hresult(scaler->Initialize(source.get(), outWidth, outHeight, WICBitmapInterpolationModeFant));
        source = scaler.as<IWICBitmapSource>();
    }
    winrt::com_ptr<IWICFormatConverter> converter;
    winrt::check_hresult(factory->CreateFormatConverter(converter.put()));
    winrt::check_hresult(converter->Initialize(source.get(), GUID_WICPixelFormat32bppPBGRA,
        WICBitmapDitherTypeNone, nullptr, 0, WICBitmapPaletteTypeCustom));
    auto artwork = std::make_shared<MediaArtwork>();
    artwork->width = outWidth;
    artwork->height = outHeight;
    artwork->pixels.resize(static_cast<std::size_t>(outWidth) * outHeight * 4);
    winrt::check_hresult(converter->CopyPixels(nullptr, outWidth * 4,
        static_cast<UINT>(artwork->pixels.size()), artwork->pixels.data()));
    return artwork;
}
} // namespace

struct MediaSource::Impl {
    std::shared_ptr<Shared> shared = std::make_shared<Shared>();
    std::thread worker;
    // Everything below is touched only by the owned MTA worker.
    struct Entry {
        Session session{nullptr};
        winrt::com_ptr<::IUnknown> identity; // COM identity; never SourceAppUserModelId.
        std::uint64_t id{};
        std::wstring source;
        bool seen{};
        std::optional<winrt::event_token> playback;
    };
    std::vector<Entry> sessions;
    std::uint64_t nextId{}, selectedId{};
    Manager manager{nullptr};
    Session selected{nullptr};
    std::optional<winrt::event_token> sessionsChanged, currentChanged, mediaChanged, timelineChanged;
    IAsyncOperation<Manager> request{nullptr};
    IAsyncOperation<Properties> metadata{nullptr};
    IAsyncOperation<IRandomAccessStreamWithContentType> open{nullptr};
    DataReaderLoadOperation load{nullptr};
    DataReader reader{nullptr};
    unsigned byteCount{};
    MediaSnapshot snapshot;

    // Stale = stopped, a newer revision exists, or a track change is pending.
    bool StaleLocked() const {
        return shared->stopping || (shared->dirty & FlagMetadata) ||
            !media_detail::Current(snapshot.sessionId, snapshot.revision, shared->selected, shared->revision);
    }
    bool Stale() {
        std::lock_guard lock(shared->mutex);
        return StaleLocked();
    }
    void Publish() {
        auto value = std::make_shared<const MediaSnapshot>(snapshot);
        std::lock_guard lock(shared->mutex);
        if (StaleLocked()) return;
        shared->latest = shared->last = std::move(value);
        SetEvent(shared->changed.value);
    }
    void CancelMedia() noexcept {
        Cancel(metadata);
        Cancel(open);
        Cancel(load);
        reader = nullptr;
        byteCount = 0;
    }
    void Unselect() noexcept {
        if (selected && mediaChanged) {
            try { selected.MediaPropertiesChanged(*mediaChanged); } catch (...) { LogFailure(); }
        }
        if (selected && timelineChanged) {
            try { selected.TimelinePropertiesChanged(*timelineChanged); } catch (...) { LogFailure(); }
        }
        mediaChanged.reset();
        timelineChanged.reset();
        selected = nullptr;
    }
    static void Unwatch(Entry& entry) noexcept {
        if (entry.playback) {
            try { entry.session.PlaybackInfoChanged(*entry.playback); } catch (...) { LogFailure(); }
            entry.playback.reset();
        }
    }
    void Cleanup() noexcept {
        Cancel(request);
        CancelMedia();
        Unselect();
        for (auto& entry : sessions) Unwatch(entry);
        sessions.clear();
        if (manager && sessionsChanged) {
            try { manager.SessionsChanged(*sessionsChanged); } catch (...) { LogFailure(); }
        }
        if (manager && currentChanged) {
            try { manager.CurrentSessionChanged(*currentChanged); } catch (...) { LogFailure(); }
        }
        sessionsChanged.reset();
        currentChanged.reset();
        manager = nullptr;
    }
    void SetSelection(const Entry* entry) {
        CancelMedia();
        Unselect();
        selectedId = entry ? entry->id : 0;
        {
            std::lock_guard lock(shared->mutex);
            shared->selected = selectedId;
            shared->ClearLocked(false);
        }
        if (entry) {
            selected = entry->session;
            mediaChanged = selected.MediaPropertiesChanged(Notify(shared, FlagMetadata, selectedId));
            timelineChanged = selected.TimelinePropertiesChanged(Notify(shared, FlagTimeline, selectedId));
        }
    }
    bool RefreshSelection(bool enumerate) {
        if (enumerate) {
            for (auto& entry : sessions) entry.seen = false;
            for (const auto& session : manager.GetSessions()) {
                auto identity = session.as<::IUnknown>();
                auto found = std::find_if(sessions.begin(), sessions.end(),
                    [&](const Entry& entry) { return entry.identity == identity; });
                if (found == sessions.end()) {
                    Entry entry;
                    entry.session = session;
                    entry.identity = std::move(identity);
                    entry.id = ++nextId;
                    try { entry.source = session.SourceAppUserModelId(); } catch (...) { LogFailure(); }
                    sessions.push_back(std::move(entry)); // Owned before subscribing: always revocable.
                    found = std::prev(sessions.end());
                    // All candidates: a background session can become the Playing fallback.
                    found->playback = session.PlaybackInfoChanged(Notify(shared, FlagSelection | FlagPlayback));
                }
                found->seen = true;
            }
            for (auto& entry : sessions) if (!entry.seen) Unwatch(entry);
            std::erase_if(sessions, [](const Entry& entry) { return !entry.seen; });
        }
        const auto current = manager.GetCurrentSession();
        const auto currentIdentity = current ? current.as<::IUnknown>() : winrt::com_ptr<::IUnknown>{};
        std::uint64_t currentId{};
        std::vector<media_detail::Candidate> candidates;
        candidates.reserve(sessions.size());
        for (const auto& entry : sessions) {
            if (currentIdentity && entry.identity == currentIdentity) currentId = entry.id;
            candidates.push_back({entry.id, entry.source, IsPlaying(entry.session)});
        }
        const auto id = media_detail::Select(candidates, currentId, selectedId);
        if (id == selectedId) return false;
        const auto found = std::find_if(sessions.begin(), sessions.end(),
            [id](const Entry& entry) { return entry.id == id; });
        SetSelection(found == sessions.end() ? nullptr : &*found);
        return true;
    }
    void ReadTimeline() noexcept {
        snapshot.positionSeconds = snapshot.durationSeconds = 0;
        if (!selected) return;
        try {
            const auto info = selected.GetTimelineProperties();
            if (info) media_detail::Timeline(snapshot, info.StartTime().count(), info.EndTime().count(),
                                             info.Position().count());
        } catch (...) { LogFailure(); }
    }
    void RefreshMetadata() {
        CancelMedia();
        snapshot = {};
        snapshot.sessionId = selectedId;
        {
            std::lock_guard lock(shared->mutex);
            snapshot.revision = shared->revision;
        }
        snapshot.available = static_cast<bool>(selected);
        if (!selected) { Publish(); return; }
        try { snapshot.source = selected.SourceAppUserModelId(); } catch (...) { LogFailure(); }
        snapshot.playing = IsPlaying(selected);
        ReadTimeline();
        Publish(); // Source/playback now; title/artist/artwork follow asynchronously.
        metadata = selected.TryGetMediaPropertiesAsync();
        OnCompletion(metadata, shared);
    }
    // Advances the metadata -> thumbnail open -> bounded load -> decode chain; never blocks.
    void CompleteMedia() {
        if (Stale()) { CancelMedia(); return; }
        if (Ready(metadata)) {
            const auto properties = std::exchange(metadata, nullptr).GetResults();
            if (!properties) return;
            snapshot.title = properties.Title();
            snapshot.artist = properties.Artist();
            Publish();
            if (const auto thumbnail = properties.Thumbnail()) {
                open = thumbnail.OpenReadAsync();
                OnCompletion(open, shared);
            }
        }
        if (Ready(open)) {
            const auto stream = std::exchange(open, nullptr).GetResults();
            if (!stream) return;
            const auto size = stream.Size();
            if (!media_detail::EncodedSize(size)) {
                Log("GSMTC artwork rejected: encoded size empty or over 8 MiB");
                return;
            }
            byteCount = static_cast<unsigned>(size);
            reader = DataReader(stream.GetInputStreamAt(0));
            load = reader.LoadAsync(byteCount);
            OnCompletion(load, shared);
        }
        if (Ready(load)) {
            const auto read = std::exchange(load, nullptr).GetResults();
            auto bytes = std::vector<std::uint8_t>(read == byteCount ? byteCount : 0);
            if (!bytes.empty()) reader.ReadBytes(bytes);
            reader = nullptr;
            if (bytes.empty()) { Log("GSMTC artwork rejected: truncated stream"); return; }
            if (Stale()) return;
            snapshot.artwork = Decode(bytes);
            Publish(); // Rechecks session identity AND revision after decoding.
        }
    }
    void Step(unsigned flags) {
        if (Ready(request)) {
            manager = std::exchange(request, nullptr).GetResults();
            if (manager) {
                sessionsChanged = manager.SessionsChanged(Notify(shared, FlagEnumerate));
                currentChanged = manager.CurrentSessionChanged(Notify(shared, FlagEnumerate));
                flags |= FlagEnumerate | FlagMetadata;
            }
        }
        const bool changed = manager && (flags & (FlagEnumerate | FlagSelection)) &&
            RefreshSelection((flags & FlagEnumerate) != 0);
        if (changed || (flags & FlagMetadata)) RefreshMetadata();
        else if (flags & (FlagPlayback | FlagTimeline)) {
            if ((flags & FlagPlayback) && selected) snapshot.playing = IsPlaying(selected);
            if (flags & FlagTimeline) ReadTimeline();
            Publish();
        }
        CompleteMedia();
    }
    void Run() noexcept {
        bool apartment{};
        try {
            winrt::init_apartment(winrt::apartment_type::multi_threaded);
            apartment = true;
            request = Manager::RequestAsync();
            OnCompletion(request, shared);
            for (;;) {
                if (WaitForSingleObject(shared->wake.value, INFINITE) != WAIT_OBJECT_0) {
                    Log("GSMTC worker wait failed");
                    break;
                }
                unsigned flags{};
                {
                    std::lock_guard lock(shared->mutex);
                    if (shared->stopping) break;
                    flags = std::exchange(shared->dirty, 0U);
                }
                try { Step(flags); }
                catch (...) {
                    LogFailure();
                    CancelMedia();
                    // Keep partial data (e.g. title without artwork); Publish rejects old revisions.
                    try { Publish(); } catch (...) { LogFailure(); }
                }
            }
        } catch (...) { LogFailure(); }
        Cleanup();
        if (apartment) winrt::uninit_apartment();
    }
};

MediaSource::MediaSource() : impl_(std::make_unique<Impl>()) {}
MediaSource::~MediaSource() { Stop(); }
void MediaSource::Start() {
    if (impl_->worker.joinable()) return;
    {
        std::lock_guard lock(impl_->shared->mutex);
        impl_->shared->stopping = false;
        impl_->shared->dirty = 0;
    }
    impl_->selectedId = 0;
    impl_->snapshot = {};
    impl_->worker = std::thread([impl = impl_.get()] { impl->Run(); });
}
void MediaSource::Stop() noexcept {
    {
        std::lock_guard lock(impl_->shared->mutex);
        impl_->shared->stopping = true;
        impl_->shared->selected = 0;
        try { impl_->shared->ClearLocked(false); }
        catch (...) { ++impl_->shared->revision; impl_->shared->latest.reset(); impl_->shared->last.reset(); }
        SetEvent(impl_->shared->wake.value);
    }
    // Worker cancels operations, revokes every event token, then exits: an unbounded but event-driven join.
    if (impl_->worker.joinable()) impl_->worker.join();
}
HANDLE MediaSource::Changed() const noexcept { return impl_->shared->changed.value; }
std::shared_ptr<const MediaSnapshot> MediaSource::Take() {
    std::lock_guard lock(impl_->shared->mutex);
    ResetEvent(impl_->shared->changed.value);
    return std::exchange(impl_->shared->latest, {});
}
} // namespace caecilia

#include "Editor/Internals/Platform/Windows/SharedFrames.h"
#include <windows.h>
#include <algorithm>
#include <array>
#include <cstring>
#include <stdexcept>
#include <type_traits>
#include "Editor/Internals/Platform/Windows/NativeHandle.h"
#include "Editor/Internals/Platform/Windows/WindowsText.h"
#include "Editor/Internals/Protocol/PreviewProtocol.h"

namespace {
constexpr std::size_t SlotBytes = static_cast<std::size_t>(PreviewMaxFrameWidth) * PreviewMaxFrameHeight * 4;
constexpr std::uint32_t FrameMagic = 0x4546524d;
struct FrameInfo {
    std::uint64_t generation = 0;
    std::uint64_t sequence = 0;
    double capturedAt = 0.0;
    std::uint32_t width = 0;
    std::uint32_t height = 0;
    std::uint32_t stride = 0;
    std::uint32_t format = 1; // top-to-bottom RGBA8
};
struct FrameHeader {
    std::uint32_t magic = FrameMagic;
    std::uint32_t version = PreviewProtocolVersion;
    std::array<char, 32> session{};
    std::uint64_t sequence = 0;
    std::uint32_t latest = 0;
    std::uint32_t capacity = static_cast<std::uint32_t>(SlotBytes);
    std::array<FrameInfo, 2> slots{};
};
static_assert(std::is_trivially_copyable_v<FrameHeader>);
constexpr std::size_t MappingBytes = sizeof(FrameHeader) + 2 * SlotBytes;
struct UnmapView {
    void operator()(std::byte* pointer) const {
        if (pointer) UnmapViewOfFile(pointer);
    }
};
// This lock owns a kernel resource, never C++ cross-process atomics or object pointers.
struct FrameLock {
    explicit FrameLock(HANDLE handle)
        : mutex(handle)
        , owned(false) {
        const auto result = WaitForSingleObject(mutex, 0);
        if (result == WAIT_ABANDONED) {
            ReleaseMutex(mutex);
            throw std::runtime_error("Preview frame writer abandoned its buffer");
        }
        if (result == WAIT_FAILED) ThrowPreviewWindowsError("Lock preview frames");
        owned = result == WAIT_OBJECT_0;
    }
    ~FrameLock() {
        if (owned) ReleaseMutex(mutex);
    }
    HANDLE mutex;
    bool owned;
    FrameLock(const FrameLock&) = delete;
    FrameLock& operator=(const FrameLock&) = delete;
};
} // namespace

struct SharedFrames::State {
    // View is unmapped before its backing mapping handle closes.
    NativeHandle mapping;
    NativeHandle mutex;
    std::unique_ptr<std::byte, UnmapView> view;
    std::string session;
};

double PreviewTimestamp() {
    LARGE_INTEGER counter{};
    LARGE_INTEGER frequency{};
    QueryPerformanceCounter(&counter);
    QueryPerformanceFrequency(&frequency);
    return static_cast<double>(counter.QuadPart) / static_cast<double>(frequency.QuadPart);
}
SharedFrames::SharedFrames(const std::string& session, bool owner)
    : _state(std::make_unique<State>()) {
    if (session.size() != 32 || session.find_first_not_of("0123456789abcdef") != std::string::npos)
        throw std::runtime_error("Invalid frame session");
    _state->session = session;
    const auto name = PreviewWide("Local\\EngineEditor.Frames." + session);
    const auto lockName = name + L".Mutex";
    if (owner) {
        _state->mutex.Reset(CreateMutexW(nullptr, FALSE, lockName.c_str()));
        if (!_state->mutex) ThrowPreviewWindowsError("Create frame mutex");
        if (GetLastError() == ERROR_ALREADY_EXISTS) throw std::runtime_error("Frame session already exists");
        _state->mapping.Reset(CreateFileMappingW(INVALID_HANDLE_VALUE, nullptr, PAGE_READWRITE, 0,
                                                 static_cast<DWORD>(MappingBytes), name.c_str()));
        if (!_state->mapping) ThrowPreviewWindowsError("Create frame mapping");
        if (GetLastError() == ERROR_ALREADY_EXISTS) throw std::runtime_error("Frame mapping already exists");
    } else {
        _state->mutex.Reset(OpenMutexW(SYNCHRONIZE | MUTEX_MODIFY_STATE, FALSE, lockName.c_str()));
        _state->mapping.Reset(OpenFileMappingW(FILE_MAP_ALL_ACCESS, FALSE, name.c_str()));
    }
    if (!_state->mapping || !_state->mutex) ThrowPreviewWindowsError("Open preview frames");
    _state->view.reset(
        static_cast<std::byte*>(MapViewOfFile(_state->mapping.Get(), FILE_MAP_ALL_ACCESS, 0, 0, MappingBytes)));
    if (!_state->view) ThrowPreviewWindowsError("Map preview frames");
    if (owner) {
        FrameHeader header;
        std::copy(session.begin(), session.end(), header.session.begin());
        std::memcpy(_state->view.get(), &header, sizeof(header));
    }
}
SharedFrames::~SharedFrames() = default;
bool SharedFrames::Publish(std::uint64_t generation, int width, int height, std::span<const std::byte> rgba) {
    if (width <= 0 || height <= 0 || width > PreviewMaxFrameWidth || height > PreviewMaxFrameHeight ||
        rgba.size() != static_cast<std::size_t>(width) * static_cast<std::size_t>(height) * 4)
        throw std::runtime_error("Preview frame exceeds RGBA8 capacity");
    FrameLock lock(_state->mutex.Get());
    if (!lock.owned) return false;
    FrameHeader header;
    std::memcpy(&header, _state->view.get(), sizeof(header));
    if (header.magic != FrameMagic || header.version != PreviewProtocolVersion || header.capacity != SlotBytes ||
        !std::equal(header.session.begin(), header.session.end(), _state->session.begin()) || header.latest > 1)
        throw std::runtime_error("Invalid shared frame header");
    const std::uint32_t slot = 1 - header.latest;
    std::memcpy(_state->view.get() + sizeof(header) + static_cast<std::size_t>(slot) * SlotBytes, rgba.data(),
                rgba.size());
    header.slots[slot] = {generation,
                          ++header.sequence,
                          PreviewTimestamp(),
                          static_cast<std::uint32_t>(width),
                          static_cast<std::uint32_t>(height),
                          static_cast<std::uint32_t>(width * 4),
                          1};
    header.latest = slot;
    std::memcpy(_state->view.get(), &header, sizeof(header));
    return true;
}
std::optional<PreviewPixels> SharedFrames::Read(std::uint64_t generation) {
    FrameLock lock(_state->mutex.Get());
    if (!lock.owned) return std::nullopt;
    FrameHeader header;
    std::memcpy(&header, _state->view.get(), sizeof(header));
    if (header.magic != FrameMagic || header.version != PreviewProtocolVersion || header.capacity != SlotBytes ||
        !std::equal(header.session.begin(), header.session.end(), _state->session.begin()) || header.latest > 1)
        throw std::runtime_error("Invalid shared frame session/header");
    const auto& info = header.slots[header.latest];
    if (!info.sequence || info.sequence <= _lastSequence || info.generation != generation) return std::nullopt;
    if (!info.width || !info.height || info.width > PreviewMaxFrameWidth || info.height > PreviewMaxFrameHeight ||
        info.stride != info.width * 4 || info.format != 1)
        throw std::runtime_error("Invalid shared pixel layout");
    PreviewPixels frame;
    frame.width = static_cast<int>(info.width);
    frame.height = static_cast<int>(info.height);
    frame.sequence = info.sequence;
    frame.capturedAt = info.capturedAt;
    frame.rgba.resize(static_cast<std::size_t>(info.stride) * info.height);
    std::memcpy(frame.rgba.data(),
                _state->view.get() + sizeof(header) + static_cast<std::size_t>(header.latest) * SlotBytes,
                frame.rgba.size());
    _lastSequence = info.sequence;
    return frame;
}

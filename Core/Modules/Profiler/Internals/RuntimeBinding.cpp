#include "Modules/Profiler/Internals/RuntimeBinding.h"
#include <cstdlib>
#include <fstream>
#include <iomanip>
#include <sstream>
#include <thread>
#include <stdexcept>
#include "Runtime/Game.h"
#ifdef _WIN32
#define NOGDI
#define NOUSER
#ifndef NOMINMAX
#define NOMINMAX
#endif
#ifndef WIN32_LEAN_AND_MEAN
#define WIN32_LEAN_AND_MEAN
#endif
#include <windows.h>
#endif

ProfilerRuntimeBinding::ProfilerRuntimeBinding(Profiler& profiler, Game& game)
    : _profiler(profiler)
    , _game(game) {
    constexpr std::array Names{"Input",      "Audio",          "Application", "Scene update",
                               "Canvas CPU", "Preview bridge", "Present",     "EndDrawing / wait"};
    for (std::size_t i = 0; i < Names.size(); ++i)
        _regions[i] = profiler.RegisterScope(Names[i]);
    game.ObserveFrames(this);
}
ProfilerRuntimeBinding::~ProfilerRuntimeBinding() { _game.ObserveFrames(nullptr); }
void ProfilerRuntimeBinding::BeginFrame() noexcept {
    _active = _profiler.Enabled();
    if (!_active) return;
    try {
        Poll();
        _profiler.BeginFrame();
    } catch (...) {
        _active = false;
    }
}
void ProfilerRuntimeBinding::EndFrame() noexcept {
    if (!_active) return;
    try {
        _profiler.EndFrame();
    } catch (...) {
    }
    _active = false;
}
void ProfilerRuntimeBinding::BeginRegion(FrameRegion region) noexcept {
    if (!_active) return;
    try {
        _profiler.BeginScope(_regions[static_cast<std::size_t>(region)]);
        if (region == FrameRegion::Canvas) _gpu.Begin(_profiler.FrameId());
    } catch (...) {
    }
}
void ProfilerRuntimeBinding::EndRegion(FrameRegion region) noexcept {
    if (!_active) return;
    try {
        if (region == FrameRegion::Canvas) _gpu.End();
        _profiler.EndScope();
    } catch (...) {
    }
}
void ProfilerRuntimeBinding::Poll() {
    _gpu.Poll([this](std::uint64_t id, double ms) { _profiler.SetGpuTime(id, ms); });
}
Profiler::Metadata ProfilerRuntimeBinding::MetadataValue() const {
    Profiler::Metadata metadata;
    metadata.gpu = _gpu.Description();
#ifdef NDEBUG
    metadata.build = "Release";
#else
    metadata.build = "Debug";
#endif
#ifdef _WIN32
    std::array<char, 256> cpu{};
    DWORD bytes = static_cast<DWORD>(cpu.size());
    if (RegGetValueA(HKEY_LOCAL_MACHINE, "HARDWARE\\DESCRIPTION\\System\\CentralProcessor\\0", "ProcessorNameString",
                     RRF_RT_REG_SZ, nullptr, cpu.data(), &bytes) == ERROR_SUCCESS)
        metadata.cpu = cpu.data();
    std::array<char, 256> machine{};
    DWORD count = static_cast<DWORD>(machine.size());
    if (GetComputerNameA(machine.data(), &count)) metadata.machine = machine.data();
    metadata.environment =
        "Windows / " + metadata.gpu + " / threads=" + std::to_string(std::thread::hardware_concurrency());
    std::array<wchar_t, 32768> path{};
    const DWORD length = GetModuleFileNameW(nullptr, path.data(), static_cast<DWORD>(path.size()));
    if (length && length < path.size()) {
        std::ifstream binary(std::filesystem::path(std::wstring(path.data(), length)), std::ios::binary);
        std::uint64_t hash = 14695981039346656037ULL;
        std::array<char, 16384> buffer{};
        while (binary) {
            binary.read(buffer.data(), static_cast<std::streamsize>(buffer.size()));
            for (std::streamsize i = 0; i < binary.gcount(); ++i) {
                hash ^= static_cast<unsigned char>(buffer[static_cast<std::size_t>(i)]);
                hash *= 1099511628211ULL;
            }
        }
        std::ostringstream label;
        label << std::hex << hash;
        metadata.binary = label.str();
    }
#else
    metadata.environment = "CPU-only runtime";
#endif
    return metadata;
}
void Profiler::Attach(Game& game) {
    if (_runtime) throw std::logic_error("Profiler already attached");
    if (game.FrameInstrumentation()) throw std::logic_error("Game already has a frame observer");
    _runtime = std::make_unique<ProfilerRuntimeBinding>(*this, game);
}
void Profiler::Detach() { _runtime.reset(); }
void Profiler::PollGpu() {
    if (_runtime) _runtime->Poll();
}
bool Profiler::GpuSupported() const { return _runtime && _runtime->Supported(); }
std::size_t Profiler::PendingGpu() const { return _runtime ? _runtime->Pending() : 0; }
Profiler::Metadata Profiler::RuntimeMetadata() const { return _runtime ? _runtime->MetadataValue() : Metadata{}; }

#include "Modules/Profiler/Internals/GpuTimer.h"
#include <array>
#include <bit>
#include <cstddef>
#include <sstream>
#ifdef _WIN32
#define NOMINMAX
#define WIN32_LEAN_AND_MEAN
#include <windows.h>
#include <GL/gl.h>
#endif

struct GpuTimer::State {
    bool supported = false;
    int active = -1;
    std::string description = "Unavailable";
#ifdef _WIN32
    using Gen = void(APIENTRY*)(GLsizei, GLuint*);
    using Delete = void(APIENTRY*)(GLsizei, const GLuint*);
    using Timestamp = void(APIENTRY*)(GLuint, GLenum);
    using Available = void(APIENTRY*)(GLuint, GLenum, GLint*);
    using Result = void(APIENTRY*)(GLuint, GLenum, std::uint64_t*);
    Gen gen = nullptr;
    Delete remove = nullptr;
    Timestamp stamp = nullptr;
    Available available = nullptr;
    Result result = nullptr;
    struct Pair {
        GLuint start = 0, end = 0;
        std::uint64_t frame = 0;
        bool ended = false;
    };
    std::array<Pair, 8> pairs{};
#endif
};

GpuTimer::GpuTimer()
    : _state(std::make_unique<State>()) {
#ifdef _WIN32
    const auto load = [](const char* name) -> PROC {
        const auto proc = wglGetProcAddress(name);
        const auto address = reinterpret_cast<std::intptr_t>(proc);
        if (address == 0 || address == 1 || address == 2 || address == 3 || address == -1) return nullptr;
        return proc;
    };
    _state->gen = std::bit_cast<State::Gen>(load("glGenQueries"));
    _state->remove = std::bit_cast<State::Delete>(load("glDeleteQueries"));
    _state->stamp = std::bit_cast<State::Timestamp>(load("glQueryCounter"));
    _state->available = std::bit_cast<State::Available>(load("glGetQueryObjectiv"));
    _state->result = std::bit_cast<State::Result>(load("glGetQueryObjectui64v"));
    const auto text = [](GLenum field) {
        const auto* value = glGetString(field);
        return value ? std::string(reinterpret_cast<const char*>(value)) : std::string("Unknown");
    };
    _state->description = text(GL_VENDOR) + " / " + text(GL_RENDERER) + " / " + text(GL_VERSION);
    int major = 0, minor = 0;
    char separator = 0;
    std::istringstream version(text(GL_VERSION));
    version >> major >> separator >> minor;
    _state->supported = (major > 3 || (major == 3 && minor >= 3)) && _state->gen && _state->remove && _state->stamp &&
                        _state->available && _state->result;
    using Bits = void(APIENTRY*)(GLenum, GLenum, GLint*);
    const auto bits = std::bit_cast<Bits>(load("glGetQueryiv"));
    if (_state->supported && bits) {
        constexpr GLenum Timestamp = 0x8E28, CounterBits = 0x8864;
        GLint precision = 0;
        bits(Timestamp, CounterBits, &precision);
        _state->supported = precision > 0;
    } else
        _state->supported = false;
#endif
}
GpuTimer::~GpuTimer() {
#ifdef _WIN32
    if (_state->remove)
        for (const auto& pair : _state->pairs) {
            if (pair.start) _state->remove(1, &pair.start);
            if (pair.end) _state->remove(1, &pair.end);
        }
#endif
}
void GpuTimer::Begin(std::uint64_t frame) {
#ifdef _WIN32
    if (!_state->supported || _state->active >= 0) return;
    if (!_state->pairs.front().start)
        for (auto& pair : _state->pairs) {
            _state->gen(1, &pair.start);
            _state->gen(1, &pair.end);
        }
    for (std::size_t i = 0; i < _state->pairs.size(); ++i) {
        auto& pair = _state->pairs[i];
        if (pair.frame) continue;
        pair.frame = frame;
        pair.ended = false;
        _state->active = static_cast<int>(i);
        constexpr GLenum Timestamp = 0x8E28;
        _state->stamp(pair.start, Timestamp);
        return;
    }
#else
    (void)frame;
#endif
}
void GpuTimer::End() {
#ifdef _WIN32
    if (_state->active < 0) return;
    auto& pair = _state->pairs[static_cast<std::size_t>(_state->active)];
    constexpr GLenum Timestamp = 0x8E28;
    _state->stamp(pair.end, Timestamp);
    pair.ended = true;
    _state->active = -1;
    glFlush();
#endif
}
void GpuTimer::Poll(const std::function<void(std::uint64_t, double)>& result) {
#ifdef _WIN32
    if (!_state->supported) return;
    constexpr GLenum ResultAvailable = 0x8867, QueryResult = 0x8866;
    for (auto& pair : _state->pairs) {
        if (!pair.frame || !pair.ended) continue;
        GLint ready = GL_FALSE;
        _state->available(pair.end, ResultAvailable, &ready);
        if (ready != GL_TRUE) continue;
        std::uint64_t start = 0, end = 0;
        _state->result(pair.start, QueryResult, &start);
        _state->result(pair.end, QueryResult, &end);
        if (end >= start) result(pair.frame, static_cast<double>(end - start) / 1000000.0);
        pair.frame = 0;
        pair.ended = false;
    }
#else
    (void)result;
#endif
}
bool GpuTimer::Supported() const { return _state->supported; }
std::size_t GpuTimer::Pending() const {
    std::size_t count = 0;
#ifdef _WIN32
    for (const auto& pair : _state->pairs)
        if (pair.frame) ++count;
#endif
    return count;
}
std::string GpuTimer::Description() const { return _state->description; }

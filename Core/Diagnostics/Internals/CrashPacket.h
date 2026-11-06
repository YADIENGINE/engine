#pragma once
#ifndef WIN32_LEAN_AND_MEAN
#define WIN32_LEAN_AND_MEAN
#endif
#ifndef NOMINMAX
#define NOMINMAX
#endif
#include <windows.h>
#include <array>
#include <cstdint>

// Same engine build on both sides. No pointers to game memory, dynamic containers or locks in the crash path.
struct CrashPacket {
    static constexpr DWORD Version = 1;
    static constexpr DWORD MaxFrames = 128;
    DWORD version = Version;
    volatile LONG state = 0; // 0 starting, 1 ready, 2 reporting, 3 complete, 4 failed
    DWORD process = 0;
    DWORD thread = 0;
    DWORD code = 0;
    BOOL dialog = FALSE;
    BOOL native = FALSE;
    EXCEPTION_RECORD exception{};
    CONTEXT context{};
    DWORD frameCount = 0;
    std::array<DWORD64, MaxFrames> frames{};
    std::array<char, 1024> message{};
    std::array<char, 512> label{};
    std::array<char, 32> kind{};
    std::array<char, 128> engine{};
    std::array<wchar_t, 32768> directory{};
    std::array<wchar_t, 32768> executable{};
};

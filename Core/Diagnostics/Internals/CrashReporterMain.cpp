#include <windows.h>
#include <dbghelp.h>
#include <array>
#include <filesystem>
#include <fstream>
#include <iostream>
#include <stdexcept>
#include <string>
#include "Diagnostics/Internals/CrashReport.h"

namespace {
void WriteDump(HANDLE process, const CrashPacket& packet, nlohmann::json& report) {
    const auto path = std::filesystem::path(packet.directory.data()) / "crash.dmp";
    HANDLE file = CreateFileW(path.c_str(), GENERIC_WRITE, 0, nullptr, CREATE_ALWAYS, FILE_ATTRIBUTE_NORMAL, nullptr);
    auto record = packet.exception;
    auto context = packet.context;
    EXCEPTION_POINTERS pointers{&record, &context};
    MINIDUMP_EXCEPTION_INFORMATION exception{packet.thread, &pointers, FALSE};
    if (file == INVALID_HANDLE_VALUE) {
        report["dump_error"] = GetLastError();
        return;
    }
    if (!MiniDumpWriteDump(process, packet.process, file, MiniDumpWithThreadInfo, &exception, nullptr, nullptr))
        report["dump_error"] = GetLastError();
    CloseHandle(file);
}
int Monitor(wchar_t** argv) {
    const auto handle = [](const wchar_t* value) { return reinterpret_cast<HANDLE>(std::stoull(value)); };
    HANDLE mapping = handle(argv[2]), request = handle(argv[3]), done = handle(argv[4]), process = handle(argv[5]);
    auto* packet = static_cast<CrashPacket*>(MapViewOfFile(mapping, FILE_MAP_ALL_ACCESS, 0, 0, sizeof(CrashPacket)));
    if (!packet || packet->version != CrashPacket::Version) return 2;
    InterlockedExchange(&packet->state, 1);
    SetEvent(done);
    std::array<HANDLE, 2> waits{request, process};
    if (WaitForMultipleObjects(static_cast<DWORD>(waits.size()), waits.data(), FALSE, INFINITE) != WAIT_OBJECT_0)
        return 0;
    bool saved = false;
    try {
        nlohmann::json dump;
        WriteDump(process, *packet, dump);
        auto report = CaptureCrashReport(process, *packet);
        if (dump.contains("dump_error")) {
            report["dump_error"] = dump["dump_error"];
            report["status"] = "partial";
        }
        const std::filesystem::path directory(packet->directory.data());
        std::ofstream text(directory / "report.txt", std::ios::binary);
        text << CrashReportText(report);
        text.close();
        if (!text) throw std::runtime_error("Cannot write crash text");
        std::ofstream json(directory / "report.json", std::ios::binary);
        json << report.dump(2, ' ', false, nlohmann::json::error_handler_t::replace) << '\n';
        json.close();
        if (!json) throw std::runtime_error("Cannot write crash JSON");
        saved = true;
    } catch (...) {
        // The target already has an emergency file. Never recurse into the crashing application.
    }
    InterlockedExchange(&packet->state, saved ? 3 : 4);
    SetEvent(done);
    if (saved && packet->dialog) {
        WaitForSingleObject(process, INFINITE);
        const auto message = L"The application stopped unexpectedly. A local crash report was saved to:\n\n" +
                             std::wstring(packet->directory.data());
        MessageBoxW(nullptr, message.c_str(), L"Crash report", MB_OK | MB_ICONERROR);
    }
    UnmapViewOfFile(packet);
    CloseHandle(mapping);
    CloseHandle(request);
    CloseHandle(done);
    CloseHandle(process);
    return saved ? 0 : 1;
}
} // namespace

int wmain(int argc, wchar_t** argv) {
    SetErrorMode(SEM_FAILCRITICALERRORS | SEM_NOGPFAULTERRORBOX);
    try {
        if (argc == 6 && std::wstring_view(argv[1]) == L"--monitor") return Monitor(argv);
        if (argc < 3 || std::wstring_view(argv[1]) != L"--resolve")
            throw std::runtime_error("usage: --resolve report.json [--symbols directory] [--json]");
        std::filesystem::path symbols;
        bool jsonOutput = false;
        for (int i = 3; i < argc; ++i) {
            if (std::wstring_view(argv[i]) == L"--symbols" && i + 1 < argc)
                symbols = argv[++i];
            else if (std::wstring_view(argv[i]) == L"--json")
                jsonOutput = true;
            else
                throw std::runtime_error("Unknown crash reporter argument");
        }
        std::ifstream input{std::filesystem::path(argv[2])};
        auto report = nlohmann::json::parse(input);
        ResolveCrashReport(report, GetCurrentProcess(), symbols);
        if (jsonOutput)
            std::cout << report.dump(2, ' ', false, nlohmann::json::error_handler_t::replace) << '\n';
        else
            std::cout << CrashReportText(report);
        return 0;
    } catch (const std::exception& error) {
        std::cerr << "error: " << error.what() << '\n';
        return 2;
    }
}

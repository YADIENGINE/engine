#include "Diagnostics/Internals/CrashReport.h"
#include <dbghelp.h>
#include <tlhelp32.h>
#include <algorithm>
#include <array>
#include <cstring>
#include <format>
#include <sstream>
#include <stdexcept>

namespace {
std::string Utf8(const std::filesystem::path& path) {
    const auto bytes = path.u8string();
    return {reinterpret_cast<const char*>(bytes.data()), bytes.size()};
}
std::filesystem::path Path(const std::string& value) {
    return std::filesystem::path(std::u8string_view(reinterpret_cast<const char8_t*>(value.data()), value.size()));
}
std::string Guid(const GUID& guid) {
    return std::format("{:08X}{:04X}{:04X}{:02X}{:02X}{:02X}{:02X}{:02X}{:02X}{:02X}{:02X}", guid.Data1, guid.Data2,
                       guid.Data3, guid.Data4[0], guid.Data4[1], guid.Data4[2], guid.Data4[3], guid.Data4[4],
                       guid.Data4[5], guid.Data4[6], guid.Data4[7]);
}
template <class T> bool Read(HANDLE process, DWORD64 address, T& value) {
    SIZE_T read = 0;
    return ReadProcessMemory(process, reinterpret_cast<const void*>(address), &value, sizeof(value), &read) &&
           read == sizeof(value);
}
nlohmann::json Pdb(HANDLE process, DWORD64 base) {
    IMAGE_DOS_HEADER dos{};
    IMAGE_NT_HEADERS64 nt{};
    if (!Read(process, base, dos) || dos.e_magic != IMAGE_DOS_SIGNATURE || dos.e_lfanew <= 0 ||
        !Read(process, base + dos.e_lfanew, nt) || nt.Signature != IMAGE_NT_SIGNATURE)
        return nullptr;
    const auto debug = nt.OptionalHeader.DataDirectory[IMAGE_DIRECTORY_ENTRY_DEBUG];
    const auto count = std::min<DWORD>(debug.Size / sizeof(IMAGE_DEBUG_DIRECTORY), 64);
    for (DWORD i = 0; i < count; ++i) {
        IMAGE_DEBUG_DIRECTORY entry{};
        if (!Read(process, base + debug.VirtualAddress + i * sizeof(entry), entry) ||
            entry.Type != IMAGE_DEBUG_TYPE_CODEVIEW)
            continue;
        std::array<unsigned char, 1024> data{};
        SIZE_T read = 0;
        if (!ReadProcessMemory(process, reinterpret_cast<const void*>(base + entry.AddressOfRawData), data.data(),
                               std::min<std::size_t>(entry.SizeOfData, data.size() - 1), &read) ||
            read < 25 || std::memcmp(data.data(), "RSDS", 4) != 0)
            continue;
        GUID guid{};
        DWORD age = 0;
        std::memcpy(&guid, data.data() + 4, sizeof(guid));
        std::memcpy(&age, data.data() + 20, sizeof(age));
        const auto name = Path(reinterpret_cast<const char*>(data.data() + 24)).filename();
        return {{"guid", Guid(guid)}, {"age", age}, {"name", Utf8(name)}};
    }
    return nullptr;
}
nlohmann::json Modules(HANDLE process, DWORD pid) {
    auto modules = nlohmann::json::array();
    HANDLE snapshot = CreateToolhelp32Snapshot(TH32CS_SNAPMODULE | TH32CS_SNAPMODULE32, pid);
    if (snapshot == INVALID_HANDLE_VALUE) return modules;
    MODULEENTRY32W entry{};
    entry.dwSize = sizeof(entry);
    if (Module32FirstW(snapshot, &entry)) {
        do {
            const auto base = reinterpret_cast<DWORD64>(entry.modBaseAddr);
            modules.push_back({{"name", Utf8(std::filesystem::path(entry.szExePath).filename())},
                               {"path", Utf8(entry.szExePath)},
                               {"base", base},
                               {"size", entry.modBaseSize},
                               {"pdb", Pdb(process, base)}});
        } while (Module32NextW(snapshot, &entry));
    }
    CloseHandle(snapshot);
    return modules;
}
class Symbols {
public:
    Symbols(HANDLE process, nlohmann::json& modules, const std::filesystem::path& directory)
        : _process(process) {
        SymSetOptions(SYMOPT_LOAD_LINES | SYMOPT_UNDNAME | SYMOPT_EXACT_SYMBOLS | SYMOPT_FAIL_CRITICAL_ERRORS |
                      SYMOPT_NO_PROMPTS | SYMOPT_DEFERRED_LOADS | SYMOPT_IGNORE_NT_SYMPATH |
                      SYMOPT_DISABLE_SYMSRV_AUTODETECT);
        if (!SymInitialize(process, "", FALSE)) throw std::runtime_error("Cannot initialize local symbols");
        _initialized = true;
        for (auto& module : modules) {
            auto image = Path(module.at("path"));
            if (!directory.empty()) {
                const auto name = Path(module.at("name"));
                if (name.empty() || name.has_parent_path() || name != name.filename())
                    throw std::runtime_error("Invalid module filename in crash report");
                image = directory / name;
            }
            module["symbols"] = "missing";
            if (!std::filesystem::is_regular_file(image)) continue;
            const auto& expected = module.at("pdb");
            if (!expected.is_null()) {
                // Require the selected directory's PDB, rather than an absolute path from a developer's build.
                const auto name = Path(expected.at("name"));
                if (name.empty() || name.has_parent_path() || name != name.filename())
                    throw std::runtime_error("Invalid PDB filename in crash report");
                const auto pdb = image.parent_path() / name;
                SYMSRV_INDEX_INFOW index{};
                index.sizeofstruct = sizeof(index);
                if (!std::filesystem::is_regular_file(pdb)) continue;
                if (!SymSrvGetFileIndexInfoW(pdb.c_str(), &index, 0) ||
                    Guid(index.guid) != expected.at("guid").get<std::string>() ||
                    index.age != expected.at("age").get<DWORD>()) {
                    module["symbols"] = "mismatched";
                    continue;
                }
            }
            SymSetSearchPathW(process, image.parent_path().c_str());
            const auto base = module.at("base").get<DWORD64>();
            if (!SymLoadModuleExW(process, nullptr, image.c_str(), nullptr, base, module.at("size").get<DWORD>(),
                                  nullptr, 0))
                continue;
            IMAGEHLP_MODULE64 information{};
            information.SizeOfStruct = sizeof(information);
            // Forces deferred loading before comparing identity.
            alignas(SYMBOL_INFO) std::array<unsigned char, sizeof(SYMBOL_INFO) + MAX_SYM_NAME> storage{};
            auto* symbol = reinterpret_cast<SYMBOL_INFO*>(storage.data());
            symbol->SizeOfStruct = sizeof(SYMBOL_INFO);
            symbol->MaxNameLen = MAX_SYM_NAME;
            DWORD64 displacement = 0;
            (void)SymFromAddr(process, base, &displacement, symbol);
            if (!SymGetModuleInfo64(process, base, &information)) continue;
            if (!expected.is_null() && information.SymType == SymPdb && !information.PdbUnmatched &&
                Guid(information.PdbSig70) == expected.at("guid").get<std::string>() &&
                information.PdbAge == expected.at("age").get<DWORD>())
                module["symbols"] = "matched";
            else if (expected.is_null() && information.SymType == SymExport)
                module["symbols"] = "exports";
            else if (information.SymType == SymPdb)
                module["symbols"] = "mismatched";
        }
    }
    ~Symbols() {
        if (_initialized) SymCleanup(_process);
    }
    Symbols(const Symbols&) = delete;
    Symbols& operator=(const Symbols&) = delete;

private:
    HANDLE _process;
    bool _initialized = false;
};
nlohmann::json Frame(DWORD64 address, const nlohmann::json& modules) {
    nlohmann::json frame{{"address", address}, {"module", nullptr}, {"offset", nullptr}, {"symbols", "missing"}};
    for (std::size_t i = 0; i < modules.size(); ++i) {
        const auto base = modules[i].at("base").get<DWORD64>();
        if (address >= base && address - base < modules[i].at("size").get<DWORD64>()) {
            frame["module"] = i;
            frame["offset"] = address - base;
            break;
        }
    }
    return frame;
}
void ResolveFrames(nlohmann::json& report, HANDLE process) {
    bool partial = false;
    for (auto& frame : report.at("frames")) {
        frame.erase("function");
        frame.erase("file");
        frame.erase("line");
        frame["symbols"] = "missing";
        if (frame.at("module").is_null()) {
            partial = true;
            continue;
        }
        const auto& module = report.at("modules").at(frame.at("module").get<std::size_t>());
        frame["symbols"] = module.value("symbols", "missing");
        if (frame["symbols"] != "matched" && frame["symbols"] != "exports") {
            partial = true;
            continue;
        }
        const auto address = module.at("base").get<DWORD64>() + frame.at("offset").get<DWORD64>();
        alignas(SYMBOL_INFO) std::array<unsigned char, sizeof(SYMBOL_INFO) + MAX_SYM_NAME> storage{};
        auto* symbol = reinterpret_cast<SYMBOL_INFO*>(storage.data());
        symbol->SizeOfStruct = sizeof(SYMBOL_INFO);
        symbol->MaxNameLen = MAX_SYM_NAME;
        DWORD64 displacement = 0;
        if (SymFromAddr(process, address, &displacement, symbol))
            frame["function"] = std::string(symbol->Name, symbol->NameLen);
        IMAGEHLP_LINE64 line{};
        line.SizeOfStruct = sizeof(line);
        DWORD lineDisplacement = 0;
        if (frame["symbols"] == "matched" && SymGetLineFromAddr64(process, address, &lineDisplacement, &line)) {
            frame["file"] = line.FileName;
            frame["line"] = line.LineNumber;
        } else {
            partial = true;
        }
    }
    report["status"] = partial || report.at("frames").empty() ? "partial" : "complete";
}
} // namespace

nlohmann::json CaptureCrashReport(HANDLE process, const CrashPacket& packet) {
    SYSTEMTIME time{};
    GetSystemTime(&time);
    nlohmann::json report{{"schema_version", 1},
                          {"engine", packet.engine.data()},
                          {"kind", packet.kind.data()},
                          {"process", packet.process},
                          {"thread", packet.thread},
                          {"code", packet.code},
                          {"message", packet.message.data()},
                          {"context", packet.label.data()},
                          {"executable", Utf8(packet.executable.data())},
                          {"trace_origin", packet.frameCount ? "throw" : "fatal-handler"},
                          {"timestamp", std::format("{:04}-{:02}-{:02}T{:02}:{:02}:{:02}Z", time.wYear, time.wMonth,
                                                    time.wDay, time.wHour, time.wMinute, time.wSecond)},
                          {"frames", nlohmann::json::array()},
                          {"modules", Modules(process, packet.process)}};
    if (packet.native) report["trace_origin"] = "exception-context";
    Symbols symbols(process, report["modules"], {});
    if (packet.frameCount) {
        for (DWORD i = 0; i < std::min(packet.frameCount, CrashPacket::MaxFrames); ++i)
            report["frames"].push_back(Frame(packet.frames[i] ? packet.frames[i] - 1 : 0, report["modules"]));
    } else {
        CONTEXT context = packet.context;
        STACKFRAME64 frame{};
        frame.AddrPC = {context.Rip, 0, AddrModeFlat};
        frame.AddrFrame = {context.Rbp, 0, AddrModeFlat};
        frame.AddrStack = {context.Rsp, 0, AddrModeFlat};
        HANDLE thread = OpenThread(THREAD_GET_CONTEXT | THREAD_QUERY_INFORMATION, FALSE, packet.thread);
        report["frames"].push_back(Frame(context.Rip, report["modules"]));
        for (DWORD i = 1; thread && i < CrashPacket::MaxFrames; ++i) {
            if (!StackWalk64(IMAGE_FILE_MACHINE_AMD64, process, thread, &frame, &context, nullptr,
                             SymFunctionTableAccess64, SymGetModuleBase64, nullptr) ||
                !frame.AddrPC.Offset)
                break;
            report["frames"].push_back(Frame(frame.AddrPC.Offset - 1, report["modules"]));
            if (!frame.AddrReturn.Offset) break;
        }
        if (thread) CloseHandle(thread);
    }
    ResolveFrames(report, process);
    return report;
}
void ResolveCrashReport(nlohmann::json& report, HANDLE process, const std::filesystem::path& directory) {
    if (directory.empty()) throw std::runtime_error("A local symbol directory is required");
    if (report.at("schema_version") != 1 || !report.at("frames").is_array() || !report.at("modules").is_array())
        throw std::runtime_error("Unsupported or malformed crash report");
    Symbols symbols(process, report["modules"], directory);
    ResolveFrames(report, process);
}
std::string CrashReportText(const nlohmann::json& report) {
    std::ostringstream out;
    out << "Crash: " << report.at("message").get<std::string>() << '\n'
        << report.at("engine").get<std::string>() << " | " << report.at("kind").get<std::string>() << " | "
        << report.at("timestamp").get<std::string>() << '\n'
        << std::format("code=0x{:08X} thread={} trace={} status={}\n", report.at("code").get<DWORD>(),
                       report.at("thread").get<DWORD>(), report.at("trace_origin").get<std::string>(),
                       report.at("status").get<std::string>());
    if (!report.value("context", "").empty()) out << "Context: " << report["context"].get<std::string>() << '\n';
    for (std::size_t i = 0; i < report.at("frames").size(); ++i) {
        const auto& frame = report["frames"][i];
        out << std::format("#{:02} ", i);
        if (frame.contains("function"))
            out << frame["function"].get<std::string>();
        else if (!frame.at("module").is_null())
            out << report["modules"].at(frame["module"].get<std::size_t>()).at("name").get<std::string>()
                << std::format("+0x{:X}", frame.at("offset").get<DWORD64>());
        else
            out << std::format("0x{:X}", frame.at("address").get<DWORD64>());
        if (frame.contains("file"))
            out << "  " << frame["file"].get<std::string>() << ':' << frame["line"].get<DWORD>();
        else
            out << "  [" << frame.value("symbols", "missing") << " symbols]";
        out << '\n';
    }
    if (report.contains("dump_error")) out << "Dump unavailable: " << report["dump_error"] << '\n';
    return out.str();
}

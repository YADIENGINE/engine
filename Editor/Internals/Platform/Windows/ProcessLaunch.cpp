#include "Editor/Internals/Platform/Windows/ProcessLaunch.h"
#include <windows.h>
#include <cstddef>
#include <memory>
#include <stdexcept>
#include "Editor/Internals/Platform/Windows/WindowsText.h"

namespace {
std::wstring QuoteArgument(const std::wstring& argument) {
    if (argument.find(L'\0') != std::wstring::npos) throw std::runtime_error("NUL in process argument");
    std::wstring result = L"\"";
    std::size_t slashes = 0;
    for (wchar_t character : argument) {
        if (character == L'\\') {
            ++slashes;
            continue;
        }
        result.append(character == L'\"' ? slashes * 2 + 1 : slashes, L'\\');
        result.push_back(character);
        slashes = 0;
    }
    result.append(slashes * 2, L'\\');
    result.push_back(L'\"');
    return result;
}
std::wstring Upper(std::wstring text) {
    if (!text.empty()) CharUpperBuffW(text.data(), static_cast<DWORD>(text.size()));
    return text;
}
} // namespace

std::wstring ProcessCommandLine(const ProcessCommand& command) {
    std::wstring arguments = QuoteArgument(command.executable.wstring());
    for (const auto& argument : command.arguments)
        arguments += L" " + QuoteArgument(WorkerWide(argument));
    return arguments;
}

std::vector<wchar_t> ProcessEnvironmentBlock(const std::map<std::string, std::string>& overrides) {
    std::map<std::wstring, std::wstring> values;
    const auto release = [](wchar_t* pointer) {
        if (pointer) FreeEnvironmentStringsW(pointer);
    };
    std::unique_ptr<wchar_t, decltype(release)> block(GetEnvironmentStringsW(), release);
    if (!block) ThrowWorkerWindowsError("Read environment");
    for (const wchar_t* cursor = block.get(); *cursor;) {
        const std::wstring row(cursor);
        cursor += row.size() + 1;
        const auto separator = row.find(L'=', row.front() == L'=' ? 1 : 0);
        if (separator != std::wstring::npos) values[Upper(row.substr(0, separator))] = row.substr(separator + 1);
    }
    for (const auto& [name, value] : overrides) {
        if (name.empty() || name.find_first_of("=\0", 0, 2) != std::string::npos ||
            value.find('\0') != std::string::npos)
            throw std::runtime_error("Invalid environment override");
        values[Upper(WorkerWide(name))] = WorkerWide(value);
    }
    std::vector<wchar_t> result;
    for (const auto& [key, value] : values) {
        result.insert(result.end(), key.begin(), key.end());
        result.push_back(L'=');
        result.insert(result.end(), value.begin(), value.end());
        result.push_back(L'\0');
    }
    result.push_back(L'\0');
    if (result.size() == 1) result.push_back(L'\0');
    return result;
}

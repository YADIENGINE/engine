#include "Editor/Internals/Platform/Windows/WindowsText.h"
#include <windows.h>
#include <array>
#include <climits>
#include <cstdint>
#include <stdexcept>
#include <string_view>
#include "Platform/Internals/SecureRandom.h"

std::wstring PreviewWide(const std::string& utf8) {
    if (utf8.empty()) return {};
    if (utf8.size() > static_cast<std::size_t>(INT_MAX)) throw std::runtime_error("UTF-8 text exceeds capacity");
    const int count =
        MultiByteToWideChar(CP_UTF8, MB_ERR_INVALID_CHARS, utf8.data(), static_cast<int>(utf8.size()), nullptr, 0);
    if (!count) ThrowPreviewWindowsError("Decode UTF-8");
    std::wstring result(static_cast<std::size_t>(count), L'\0');
    if (!MultiByteToWideChar(CP_UTF8, MB_ERR_INVALID_CHARS, utf8.data(), static_cast<int>(utf8.size()), result.data(),
                             count))
        ThrowPreviewWindowsError("Decode UTF-8");
    return result;
}
std::string PreviewUtf8(const std::wstring& wide) {
    if (wide.empty()) return {};
    if (wide.size() > static_cast<std::size_t>(INT_MAX)) throw std::runtime_error("Unicode text exceeds capacity");
    const int count = WideCharToMultiByte(CP_UTF8, WC_ERR_INVALID_CHARS, wide.data(), static_cast<int>(wide.size()),
                                          nullptr, 0, nullptr, nullptr);
    if (!count) ThrowPreviewWindowsError("Encode UTF-8");
    std::string result(static_cast<std::size_t>(count), '\0');
    if (!WideCharToMultiByte(CP_UTF8, WC_ERR_INVALID_CHARS, wide.data(), static_cast<int>(wide.size()), result.data(),
                             count, nullptr, nullptr))
        ThrowPreviewWindowsError("Encode UTF-8");
    return result;
}
std::string NewPreviewSession() {
    std::array<std::uint8_t, 16> bytes{};
    FillSecureRandom(bytes);
    constexpr std::string_view Digits = "0123456789abcdef";
    std::string result;
    result.reserve(bytes.size() * 2);
    for (auto byte : bytes) {
        result.push_back(Digits[byte >> 4]);
        result.push_back(Digits[byte & 15]);
    }
    return result;
}
void ThrowPreviewWindowsError(const char* operation) {
    const DWORD code = GetLastError();
    std::array<wchar_t, 2048> buffer{};
    const DWORD length = FormatMessageW(FORMAT_MESSAGE_FROM_SYSTEM | FORMAT_MESSAGE_IGNORE_INSERTS, nullptr, code, 0,
                                        buffer.data(), static_cast<DWORD>(buffer.size()), nullptr);
    const auto description = length ? PreviewUtf8(std::wstring(buffer.data(), length)) : "Windows error";
    throw std::runtime_error(std::string(operation) + ": " + description + " (" + std::to_string(code) + ")");
}

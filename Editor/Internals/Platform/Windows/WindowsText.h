#pragma once
#include <string>

[[nodiscard]] std::wstring PreviewWide(const std::string& utf8);
[[nodiscard]] std::string PreviewUtf8(const std::wstring& wide);
[[nodiscard]] std::string NewPreviewSession();
[[noreturn]] void ThrowPreviewWindowsError(const char* operation);

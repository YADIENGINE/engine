#pragma once
#include <string>

[[nodiscard]] std::wstring WorkerWide(const std::string& utf8);
[[nodiscard]] std::string WorkerUtf8(const std::wstring& wide);
[[nodiscard]] std::string NewWorkerSession();
[[noreturn]] void ThrowWorkerWindowsError(const char* operation);

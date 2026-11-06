#pragma once
#include <nlohmann/json.hpp>
#include <filesystem>
#include <string>
#include "Diagnostics/Internals/CrashPacket.h"

[[nodiscard]] nlohmann::json CaptureCrashReport(HANDLE process, const CrashPacket& packet);
void ResolveCrashReport(nlohmann::json& report, HANDLE process, const std::filesystem::path& symbols = {});
[[nodiscard]] std::string CrashReportText(const nlohmann::json& report);

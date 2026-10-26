#pragma once
#include <filesystem>
#include <string>
#include "Modules/JsonIO/JsonIO.hpp"

// Engine owns durable gzip JSON writing; the consuming game owns every record field.
class RunStatisticsSys {
public:
    explicit RunStatisticsSys(std::filesystem::path directory);
    // Writes <run_id>.json.gz. Same ID replaces the same record atomically on retry.
    [[nodiscard]] std::filesystem::path Write(const std::string& runId, const JsonIO::Document& record) const;
    [[nodiscard]] const std::filesystem::path& Directory() const { return _directory; }
    [[nodiscard]] static std::string NewRunId();
    [[nodiscard]] static std::string UtcNow();

private:
    std::filesystem::path _directory;
};

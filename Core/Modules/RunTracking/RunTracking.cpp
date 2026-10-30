#include "Modules/RunTracking/RunTracking.hpp"
#include <algorithm>
#include <atomic>
#include <chrono>
#include <ctime>
#include <iomanip>
#include <limits>
#include <random>
#include <sstream>
#include <stdexcept>
#include <utility>

namespace {
std::tm Utc(std::time_t value) {
    std::tm result{};
#ifdef _WIN32
    gmtime_s(&result, &value);
#else
    gmtime_r(&value, &result);
#endif
    return result;
}
} // namespace

RunTracking::RunTracking(std::filesystem::path directory) {
    if (directory.empty()) throw std::invalid_argument("Run statistics directory must not be empty");
    _directory = std::filesystem::absolute(std::move(directory));
}
std::filesystem::path RunTracking::Write(const std::string& runId, const JsonIO::Document& record) const {
    if (runId.empty() || runId.size() > 120 || !std::all_of(runId.begin(), runId.end(), [](unsigned char ch) {
            return (ch >= '0' && ch <= '9') || (ch >= 'a' && ch <= 'z') || (ch >= 'A' && ch <= 'Z') || ch == '-' ||
                   ch == '_';
        }))
        throw std::invalid_argument("Invalid run ID");
    std::filesystem::create_directories(_directory);
    const auto destination = _directory / (runId + ".json.gz");
    // JsonIO serializes and compresses before atomically replacing the previous good record.
    JsonIO::Write(destination, record,
                  {.limits = {.maxBytes = std::numeric_limits<std::size_t>::max()},
                   .format = JsonIO::Format::Compact,
                   .compression = JsonIO::Compression::Gzip});
    return destination;
}
std::string RunTracking::UtcNow() {
    const auto now = std::chrono::system_clock::now();
    const auto value = Utc(std::chrono::system_clock::to_time_t(now));
    std::ostringstream output;
    output << std::put_time(&value, "%Y-%m-%dT%H:%M:%SZ");
    return output.str();
}
std::string RunTracking::NewRunId() {
    static std::atomic<unsigned> sequence{0};
    static const auto processSalt = std::random_device{}(); // Independent of all gameplay RNGs.
    const auto now = std::chrono::system_clock::now();
    const auto ticks = std::chrono::duration_cast<std::chrono::microseconds>(now.time_since_epoch()).count();
    std::ostringstream output;
    output << std::hex << ticks << '-' << processSalt << '-' << sequence.fetch_add(1);
    return output.str();
}

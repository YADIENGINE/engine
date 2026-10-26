#include "Tests/Support/TempDirectory.h"
#include <chrono>
#include <random>
#include <stdexcept>
#include <string>

TempDirectory::TempDirectory() {
    std::random_device random;
    for (int attempt = 0; attempt < 100; ++attempt) {
        const auto tick = std::chrono::steady_clock::now().time_since_epoch().count();
        _path = std::filesystem::temp_directory_path() /
                ("EngineTests-" + std::to_string(tick) + "-" + std::to_string(random()));
        if (std::filesystem::create_directory(_path)) {
            return;
        }
    }
    throw std::runtime_error("Could not create isolated test directory");
}

TempDirectory::~TempDirectory() {
    std::error_code error;
    std::filesystem::remove_all(_path, error);
}

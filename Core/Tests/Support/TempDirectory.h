#pragma once
#include <filesystem>

class TempDirectory {
public:
    TempDirectory();
    ~TempDirectory();
    [[nodiscard]] const std::filesystem::path& Path() const { return _path; }

private:
    std::filesystem::path _path;

public:
    TempDirectory(const TempDirectory&) = delete;
    TempDirectory& operator=(const TempDirectory&) = delete;
};

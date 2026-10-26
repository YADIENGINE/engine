#pragma once
#include <filesystem>
#include "Editor/Internals/Platform/Windows/NativeHandle.h"

class BuildLease {
public:
    explicit BuildLease(const std::filesystem::path& directory);
    ~BuildLease();

private:
    NativeHandle _mutex;
    bool _owned = false;

public:
    BuildLease(const BuildLease&) = delete;
    BuildLease& operator=(const BuildLease&) = delete;
};

#pragma once
#include <optional>
#include <string>
#include <string_view>

enum class BuildProblemLevel { Error, Warning };

// One compiler, linker or CMake diagnostic from a build's output.
struct BuildProblem {
    BuildProblemLevel level = BuildProblemLevel::Error;
    std::string file; // As the tool wrote it; empty when the message has no location
    int line = 0;     // 0: no line
    int column = 0;   // 0: no column
    std::string code; // "C2065", "LNK2019"; empty when the tool has none
    std::string text;
    bool operator==(const BuildProblem&) const = default;
};

// Ninja's "[12/48] ..." status.
struct BuildProgress {
    int done = 0;
    int total = 0;
};

// Recognizes MSVC, clang / gcc, the MSVC linker and CMake, as Tools/build_errors.py does for the tools' own summary.
[[nodiscard]] std::optional<BuildProblem> ParseBuildProblem(std::string_view line);
[[nodiscard]] std::optional<BuildProgress> ParseBuildProgress(std::string_view line);

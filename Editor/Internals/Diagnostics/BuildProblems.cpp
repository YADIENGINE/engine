#include "Editor/Internals/Diagnostics/BuildProblems.h"
#include <charconv>
#include <regex>

namespace {
int Number(const std::ssub_match& match) {
    int value = 0;
    if (match.matched) std::from_chars(match.first.operator->(), match.second.operator->(), value);
    return value;
}
BuildProblemLevel LevelOf(std::string_view word) {
    return word.starts_with('w') || word.starts_with('W') ? BuildProblemLevel::Warning : BuildProblemLevel::Error;
}
} // namespace

std::optional<BuildProblem> ParseBuildProblem(std::string_view line) {
    // Most output is neither; the patterns below are too slow to try on every line.
    if (line.find("rror") == std::string_view::npos && line.find("arning") == std::string_view::npos)
        return std::nullopt;
    static const std::regex Compiler(
        R"(^\s*(.+?)\((\d+)(?:,(\d+))?\)\s*:\s*(?:fatal\s+)?(error|warning)\s+([A-Z]+\d+)\s*:\s*(.*)$)");
    static const std::regex Gnu(R"(^\s*(.+?):(\d+):(\d+):\s*(?:fatal\s+)?(error|warning):\s*(.*)$)");
    static const std::regex Linker(R"(^\s*(.+?)\s*:\s*(?:fatal\s+)?(error|warning)\s+(LNK\d+)\s*:\s*(.*)$)");
    static const std::regex CMake(R"(^CMake (Error|Warning)(?: at (.+?):(\d+))?(?:\s*\([^)]*\))?:?\s*(.*)$)");
    const std::string text(line);
    std::smatch match;
    if (std::regex_match(text, match, Compiler))
        return BuildProblem{LevelOf(match[4].str()), match[1].str(), Number(match[2]),
                            Number(match[3]),        match[5].str(), match[6].str()};
    if (std::regex_match(text, match, Gnu))
        return BuildProblem{LevelOf(match[4].str()), match[1].str(), Number(match[2]), Number(match[3]), {},
                            match[5].str()};
    if (std::regex_match(text, match, Linker))
        return BuildProblem{LevelOf(match[2].str()), match[1].str(), 0, 0, match[3].str(), match[4].str()};
    if (std::regex_match(text, match, CMake))
        return BuildProblem{LevelOf(match[1].str()), match[2].str(), Number(match[3]), 0, {}, match[4].str()};
    return std::nullopt;
}

std::optional<BuildProgress> ParseBuildProgress(std::string_view line) {
    if (!line.starts_with('[')) return std::nullopt;
    const auto slash = line.find('/');
    const auto close = line.find(']');
    if (slash == std::string_view::npos || close == std::string_view::npos || slash > close) return std::nullopt;
    BuildProgress progress;
    const auto* begin = line.data();
    const auto done = std::from_chars(begin + 1, begin + slash, progress.done);
    const auto total = std::from_chars(begin + slash + 1, begin + close, progress.total);
    if (done.ec != std::errc{} || done.ptr != begin + slash || total.ec != std::errc{} || total.ptr != begin + close ||
        progress.total <= 0)
        return std::nullopt;
    return progress;
}

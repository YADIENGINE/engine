#pragma once
#include <array>
#include <cstdint>
#include "Editor/Application/EditorPresentation.h"

class ProfilerPanel {
public:
    void Draw(const EditorSnapshot& snapshot, const EditorUiActions& actions);

private:
    void Live(const EditorSnapshot& snapshot, const EditorUiActions& actions);
    void Benchmarks(const EditorSnapshot& snapshot, const EditorUiActions& actions);
    void Compare(const EditorSnapshot& snapshot, const EditorUiActions& actions);
    void Frames(const Profiler::Capture& capture);
    Profiler::Recipe _recipe;
    std::array<char, 128> _filename{'b', 'a', 's', 'e', 'l', 'i', 'n', 'e', '.', 'j', 's', 'o', 'n', '\0'};
    int _run = 0, _frame = 0;
    int _glow = -1;
    std::uint64_t _glowGeneration = 0;
    std::shared_ptr<const Profiler::Capture> _statisticsFor;
    std::vector<Profiler::Summary> _runStatistics;
    std::array<Profiler::Summary, 2> _variantStatistics;
};

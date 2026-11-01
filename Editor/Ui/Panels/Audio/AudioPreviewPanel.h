#pragma once
#include <cstdint>
#include <filesystem>
#include <optional>
#include "Editor/Application/EditorPresentation.h"

// The sound picked in Assets: its details, a waveform to seek on and the transport. It plays in the editor process
// from the source file and leaves the viewport and running scene alone.
class AudioPreviewPanel {
public:
    void SetAssetsDirectory(const std::filesystem::path& assetsDirectory) { _assetsDirectory = assetsDirectory; }
    void Draw(const EditorSnapshot& snapshot, const EditorUiActions& actions);

private:
    void Header(const AudioPreviewState& audio, const AudioUsage* usage);
    void Timeline(const AudioPreviewState& audio, const EditorUiActions& actions);
    void Transport(const AudioPreviewState& audio, const EditorUiActions& actions);
    void Refresh(const std::filesystem::path& file);

    std::filesystem::path _assetsDirectory;
    // Disk facts about the shown file, read again when it changes or after a while.
    std::filesystem::path _file;
    double _nextRefresh = 0;
    std::uintmax_t _size = 0;
    std::optional<std::filesystem::path> _previous;
    std::optional<std::filesystem::path> _next;
    std::optional<float> _scrub; // Seconds under the pointer while the timeline is held
};

#pragma once
#include <raylib.h>
#include <filesystem>
#include <map>
#include "Editor/Application/EditorPresentation.h"

// The pictures of the file open in source control's change view. Only a before and an after are on screen at a time,
// so older ones go as soon as another is asked for. A file that changed on disk loads again.
class GitImages {
public:
    GitImages() = default;
    ~GitImages();
    [[nodiscard]] EditorGitImage Load(const std::filesystem::path& file);

private:
    struct Entry {
        Texture2D texture{};
        std::filesystem::file_time_type written;
    };
    std::map<std::filesystem::path, Entry> _images;
    std::filesystem::path _before; // The last two asked for stay
    std::filesystem::path _last;

public:
    GitImages(const GitImages&) = delete;
    GitImages& operator=(const GitImages&) = delete;
};

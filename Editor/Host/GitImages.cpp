#include "Editor/Host/GitImages.h"
#include <system_error>

GitImages::~GitImages() {
    for (const auto& [file, entry] : _images)
        if (entry.texture.id) UnloadTexture(entry.texture);
}

EditorGitImage GitImages::Load(const std::filesystem::path& file) {
    std::error_code error;
    const auto written = std::filesystem::last_write_time(file, error);
    if (error) return {};
    if (file != _last) {
        _before = std::exchange(_last, file);
        for (auto entry = _images.begin(); entry != _images.end();) {
            if (entry->first == _before || entry->first == _last) {
                ++entry;
                continue;
            }
            if (entry->second.texture.id) UnloadTexture(entry->second.texture);
            entry = _images.erase(entry);
        }
    }
    auto found = _images.find(file);
    if (found != _images.end() && found->second.written != written) {
        if (found->second.texture.id) UnloadTexture(found->second.texture);
        _images.erase(found);
        found = _images.end();
    }
    if (found == _images.end()) {
        // A failed load is remembered too: it would otherwise repeat every frame.
        const auto text = file.u8string();
        const auto texture = LoadTexture(reinterpret_cast<const char*>(text.c_str()));
        found = _images.emplace(file, Entry{texture, written}).first;
    }
    const auto& texture = found->second.texture;
    return {texture.id, texture.width, texture.height};
}

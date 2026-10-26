#include "Editor/Internals/Settings/EditorPreferences.h"
#include <algorithm>

bool EditorPreferences::IsFavorite(const std::string& id) const {
    return std::ranges::find(favorites, id) != favorites.end();
}
void EditorPreferences::ToggleFavorite(const std::string& id) {
    if (IsFavorite(id))
        std::erase(favorites, id);
    else
        favorites.push_back(id);
}
void EditorPreferences::Opened(const std::string& id) {
    if (id.empty()) return;
    std::erase(recent, id);
    recent.insert(recent.begin(), id);
    constexpr std::size_t MaxRecent = 10;
    if (recent.size() > MaxRecent) recent.resize(MaxRecent);
}

#include "Editor/Internals/Session/AssetLibrary.h"
#include <algorithm>
#include <array>
#include <cctype>
#include <map>
#include <string_view>
#include <system_error>

namespace {
constexpr std::string_view RegistrationSuffix = ".asset.cpp";
constexpr std::array CodeExtensions{std::string_view(".h"), std::string_view(".hpp"), std::string_view(".cpp"),
                                    std::string_view(".json")};
constexpr std::array AudioExtensions{std::string_view(".ogg"), std::string_view(".wav"), std::string_view(".mp3"),
                                     std::string_view(".flac")};
constexpr std::array FontExtensions{std::string_view(".ttf"), std::string_view(".otf")};

std::string Lowered(std::string text) {
    std::ranges::transform(text, text.begin(), [](unsigned char c) { return static_cast<char>(std::tolower(c)); });
    return text;
}
template <std::size_t N> bool Has(const std::array<std::string_view, N>& list, const std::string& value) {
    return std::ranges::find(list, value) != list.end();
}
AssetKind FileKind(const std::string& extension) {
    if (Has(AudioExtensions, extension)) return AssetKind::Audio;
    if (Has(FontExtensions, extension)) return AssetKind::Font;
    return AssetKind::File;
}
void Sort(std::vector<AssetNode>& nodes) {
    std::ranges::sort(nodes, [](const AssetNode& a, const AssetNode& b) {
        const bool aFolder = a.kind == AssetKind::Folder;
        const bool bFolder = b.kind == AssetKind::Folder;
        if (aFolder != bFolder) return aFolder;
        return Lowered(a.name) < Lowered(b.name);
    });
}
std::vector<AssetNode> ScanFolder(const std::filesystem::path& folder, bool readOnly) {
    std::vector<AssetNode> nodes;
    std::map<std::string, AssetNode> code; // By the name before the first dot
    std::error_code error;
    for (std::filesystem::directory_iterator it(folder, error), end; !error && it != end; it.increment(error)) {
        const auto& path = it->path();
        const auto name = path.filename().string();
        if (name.starts_with('.')) continue;
        if (it->is_directory(error)) {
            nodes.push_back({name, path, AssetKind::Folder, readOnly, {}, ScanFolder(path, readOnly)});
            continue;
        }
        const auto extension = Lowered(path.extension().string());
        if (!Has(CodeExtensions, extension)) {
            nodes.push_back({path.stem().string(), path, FileKind(extension), readOnly, {name}, {}});
            continue;
        }
        auto& node = code[name.substr(0, name.find('.'))];
        node.files.push_back(name);
        if (Lowered(name).ends_with(RegistrationSuffix)) {
            node.path = path;
            node.kind = AssetKind::Graphic;
        } else if (node.kind != AssetKind::Graphic) {
            node.path = path;
            node.kind = AssetKind::Code;
        }
    }
    for (auto& [name, node] : code) {
        node.name = name;
        node.readOnly = readOnly;
        std::ranges::sort(node.files);
        nodes.push_back(std::move(node));
    }
    Sort(nodes);
    return nodes;
}
} // namespace

std::vector<AssetNode> ScanAssetLibrary(const std::filesystem::path& engineDirectory,
                                        const std::filesystem::path& assetsDirectory) {
    std::vector<AssetNode> roots;
    if (!engineDirectory.empty()) {
        const auto assets = engineDirectory / "Assets";
        const auto graphics = assets / "Graphics";
        AssetNode graphicsNode{"Graphics", graphics, AssetKind::Folder, true, {}, ScanFolder(graphics, true)};
        roots.push_back({"Engine", assets, AssetKind::Folder, true, {}, {std::move(graphicsNode)}});
    }
    if (!assetsDirectory.empty()) {
        auto children = ScanFolder(assetsDirectory, false);
        // Prefabs are reserved but not part of the library yet. Graphics always shows, as the place to start.
        std::erase_if(children, [](const AssetNode& node) { return node.name == "Prefabs"; });
        if (std::ranges::none_of(children, [](const AssetNode& node) { return node.name == "Graphics"; })) {
            children.push_back({"Graphics", assetsDirectory / "Graphics", AssetKind::Folder, false, {}, {}});
            Sort(children);
        }
        roots.push_back({"Game", assetsDirectory, AssetKind::Folder, false, {}, std::move(children)});
    }
    return roots;
}

bool IsAudioFile(const std::filesystem::path& file) { return Has(AudioExtensions, Lowered(file.extension().string())); }

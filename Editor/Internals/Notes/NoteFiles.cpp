#include "Editor/Internals/Notes/NoteFiles.h"
#include <algorithm>
#include <format>
#include <fstream>
#include <iterator>
#include <stdexcept>
#include "Editor/Internals/Platform/NotePlatform.h"
#include "Editor/Internals/Session/AssetNames.h"
#include "Platform/Internals/FileStorage.h"

namespace {
std::filesystem::path Resolved(const std::filesystem::path& path, std::error_code& error) {
    // Resolve the closest existing parent directly; walking from a drive root can require unrelated permissions.
    if (std::filesystem::exists(path, error)) return std::filesystem::canonical(path, error);
    if (error || path == path.root_path()) return {};
    const auto parent = Resolved(path.parent_path(), error);
    return error ? std::filesystem::path{} : parent / path.filename();
}
std::string Utf8(const std::filesystem::path& path) {
    const auto utf8 = path.generic_u8string();
    return {utf8.begin(), utf8.end()};
}
bool Markdown(const std::filesystem::path& path) { return IsNoteName(Utf8(path.extension())); }
bool Inside(const std::filesystem::path& candidate, const std::filesystem::path& root, bool allowRoot) {
    const auto relative = candidate.lexically_relative(root);
    return !relative.empty() && *relative.begin() != ".." && (allowRoot || relative != ".");
}
AssetNode ScanNotes(const std::filesystem::path& folder, const NoteFiles& files) {
    AssetNode node{Utf8(folder.filename()), folder, AssetKind::Folder, false, {}, {}};
    std::error_code error;
    for (std::filesystem::directory_iterator it(folder, error), end; !error && it != end; it.increment(error)) {
        if (!files.Contains(it->path()) || it->is_symlink(error)) continue;
        if (it->is_directory(error))
            node.children.push_back(ScanNotes(it->path(), files));
        else if (it->is_regular_file(error) && Markdown(it->path()))
            node.children.push_back({Utf8(it->path().filename()), it->path(), AssetKind::File, false, {}, {}});
    }
    std::ranges::sort(node.children, [](const AssetNode& a, const AssetNode& b) {
        if ((a.kind == AssetKind::Folder) != (b.kind == AssetKind::Folder)) return a.kind == AssetKind::Folder;
        return a.name < b.name;
    });
    return node;
}
std::filesystem::path NoteName(std::string name, bool directory) {
    if (!directory && !IsNoteName(name)) name += ".md";
    return std::filesystem::path(std::u8string(name.begin(), name.end()));
}
} // namespace
NoteFiles::NoteFiles(std::filesystem::path project) {
    if (!project.empty()) _root = std::filesystem::absolute(project / "Notes").lexically_normal();
}
bool NoteFiles::Contains(const std::filesystem::path& path, bool allowRoot) const {
    if (_root.empty() || path.empty()) return false;
    // Both lexical and resolved containment: never follow a Notes junction out into assets or elsewhere.
    const auto absolute = std::filesystem::absolute(path).lexically_normal();
    if (!Inside(absolute, _root, allowRoot)) return false;
    std::error_code error;
    const auto resolvedRoot = Resolved(_root, error);
    if (error) return false;
    const auto resolvedProject = Resolved(_root.parent_path(), error);
    if (error || resolvedRoot != resolvedProject / "Notes") return false;
    const auto resolved = Resolved(absolute, error);
    return !error && Inside(resolved, resolvedRoot, allowRoot);
}
bool NoteFiles::InRoot(const std::filesystem::path& path, bool allowRoot) const {
    return !_root.empty() && !path.empty() && Inside(path.lexically_normal(), _root, allowRoot);
}
void NoteFiles::Require(const std::filesystem::path& path, bool allowRoot) const {
    if (!Contains(path, allowRoot)) throw std::runtime_error("The path must stay inside Notes/");
}
AssetNode NoteFiles::Scan() const {
    if (_root.empty() || !Contains(_root, true)) return {"Notes", _root, AssetKind::Folder, false, {}, {}};
    auto node = ScanNotes(_root, *this);
    node.name = "Notes";
    return node;
}
std::optional<std::string> NoteFiles::Read(const std::filesystem::path& path) const {
    Require(path);
    if (!Markdown(path)) throw std::runtime_error("Only .md notes can be opened");
    if (!std::filesystem::exists(path)) return {};
    std::ifstream input(path, std::ios::binary);
    if (!input) throw std::runtime_error("Could not read note");
    std::string bytes{std::istreambuf_iterator<char>(input), std::istreambuf_iterator<char>()};
    if (input.bad()) throw std::runtime_error("Could not read complete note");
    return bytes;
}
void NoteFiles::Write(const std::filesystem::path& path, const std::string& bytes, bool replace) const {
    Require(path);
    if (!Markdown(path)) throw std::runtime_error("Only .md notes can be saved");
    std::filesystem::create_directories(path.parent_path());
    if (!WriteBinaryFileAtomic(path, bytes, FileAccess::Shared, replace ? WriteMode::Replace : WriteMode::CreateNew))
        throw std::runtime_error("That name already exists");
}
std::filesystem::path NoteFiles::SaveRecovery(const std::filesystem::path& note, const std::string& bytes) const {
    Require(note);
    for (int copy = 1; copy < 1000; ++copy) {
        auto file = note.stem();
        file += copy == 1 ? std::string(" (recovered).md") : std::format(" (recovered {}).md", copy);
        const auto path = note.parent_path() / file;
        if (std::filesystem::exists(path)) continue;
        Write(path, bytes, false);
        return path;
    }
    throw std::runtime_error("Too many recovered copies of this note");
}
std::string NoteFiles::NameProblem(const std::filesystem::path& folder, std::string name, bool directory,
                                   const std::filesystem::path& renaming) const {
    if (!Contains(folder, true)) return "The folder must stay inside Notes/";
    std::vector<std::string> taken;
    std::error_code error;
    for (std::filesystem::directory_iterator it(folder, error), end; !error && it != end; it.increment(error)) {
        if (!renaming.empty() && it->path().filename() == renaming.filename()) continue;
        const auto utf8 = it->path().filename().u8string();
        taken.emplace_back(utf8.begin(), utf8.end());
    }
    return AssetNameProblem(directory ? AssetNameKind::Folder : AssetNameKind::Note, name, taken);
}
std::filesystem::path NoteFiles::Create(const std::filesystem::path& folder, std::string name, bool directory) const {
    if (auto problem = NameProblem(folder, name, directory); !problem.empty()) throw std::runtime_error(problem);
    const auto path = folder / NoteName(std::move(name), directory);
    if (directory) {
        std::filesystem::create_directories(folder);
        if (!std::filesystem::create_directory(path)) throw std::runtime_error("That folder already exists");
    } else
        Write(path, "", false);
    return path;
}
std::filesystem::path NoteFiles::Rename(const std::filesystem::path& path, std::string name) const {
    Require(path);
    const bool directory = std::filesystem::is_directory(path);
    if (!directory && !Markdown(path)) throw std::runtime_error("Only .md notes can be renamed");
    if (auto problem = NameProblem(path.parent_path(), name, directory, path); !problem.empty())
        throw std::runtime_error(problem);
    const auto destination = path.parent_path() / NoteName(std::move(name), directory);
    RenameNote(path, destination);
    return destination;
}
void NoteFiles::Recycle(const std::filesystem::path& path) const {
    Require(path);
    if (!std::filesystem::is_directory(path) && !Markdown(path))
        throw std::runtime_error("Only .md notes can be deleted");
    RecycleNote(path);
}

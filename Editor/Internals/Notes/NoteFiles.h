#pragma once
#include <filesystem>
#include <optional>
#include <string>
#include "Editor/Internals/Session/AssetLibrary.h"

// All paths are checked against the configured project root's Notes folder, including resolved links.
class NoteFiles {
public:
    explicit NoteFiles(std::filesystem::path project = {});
    [[nodiscard]] const std::filesystem::path& Root() const { return _root; }
    [[nodiscard]] bool Contains(const std::filesystem::path& path, bool allowRoot = false) const;
    // Compares paths only, so it can run for every item in every frame. Anything that touches the disk uses Contains.
    [[nodiscard]] bool InRoot(const std::filesystem::path& path, bool allowRoot = false) const;
    [[nodiscard]] AssetNode Scan() const;
    [[nodiscard]] std::optional<std::string> Read(const std::filesystem::path& path) const;
    void Write(const std::filesystem::path& path, const std::string& bytes, bool replace = true) const;
    [[nodiscard]] std::filesystem::path Create(const std::filesystem::path& folder, std::string name,
                                               bool directory) const;
    [[nodiscard]] std::filesystem::path Rename(const std::filesystem::path& path, std::string name) const;
    void Recycle(const std::filesystem::path& path) const;
    // Writes `bytes` to a new "<name> (recovered).md" beside `note`, never over an existing file.
    std::filesystem::path SaveRecovery(const std::filesystem::path& note, const std::string& bytes) const;
    // `renaming` is the item being renamed: its own name does not count as taken, so its case can change.
    [[nodiscard]] std::string NameProblem(const std::filesystem::path& folder, std::string name, bool directory,
                                          const std::filesystem::path& renaming = {}) const;

private:
    void Require(const std::filesystem::path& path, bool allowRoot = false) const;
    std::filesystem::path _root;
};

#include "Editor/Internals/Session/AudioUsage.h"
#include <algorithm>
#include <cctype>
#include <format>

namespace {
std::string Comparable(const std::filesystem::path& file) {
    auto text = file.lexically_normal().generic_string();
    std::ranges::transform(text, text.begin(), [](unsigned char c) { return static_cast<char>(std::tolower(c)); });
    return text;
}
} // namespace

std::string AudioUseLabel(const AudioUse& use) {
    const char* kind = use.kind == AudioUseKind::Music ? "Music" : "Effect";
    return use.name.empty() ? std::format("{} #{}", kind, use.id) : std::format("{} {}", kind, use.name);
}

std::vector<const AudioUse*> AudioUsesOf(const AudioUsage& usage, const std::filesystem::path& file) {
    std::vector<const AudioUse*> uses;
    const auto key = Comparable(file);
    for (const auto& use : usage.uses)
        if (Comparable(use.file) == key) uses.push_back(&use);
    return uses;
}

std::string AudioUseLabels(const std::vector<const AudioUse*>& uses) {
    std::string labels;
    for (const auto* use : uses)
        labels += (labels.empty() ? "" : ", ") + AudioUseLabel(*use);
    return labels;
}

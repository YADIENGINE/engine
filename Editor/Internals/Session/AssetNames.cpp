#include "Editor/Internals/Session/AssetNames.h"
#include <algorithm>
#include <cctype>
#include <string_view>

namespace {
bool SameWithoutCase(std::string_view left, std::string_view right) {
    return std::ranges::equal(left, right,
                              [](unsigned char a, unsigned char b) { return std::tolower(a) == std::tolower(b); });
}

std::string GraphicProblem(const std::string& name) {
    if (!std::isupper(static_cast<unsigned char>(name[0]))) return "Start with a capital letter, as in PlayerShip";
    if (name.find(' ') != std::string::npos) return "No spaces: write PlayerShip, not Player Ship";
    if (!std::ranges::all_of(name, [](unsigned char c) { return std::isalnum(c) != 0; }))
        return "Use only letters and digits (no _, - or .)";
    return {};
}

std::string FolderProblem(const std::string& name, bool note = false) {
    if (name.find_first_of("/\\:") != std::string::npos) return "One folder name: no / \\ or :";
    if (name.find_first_of("<>\"|?*") != std::string::npos ||
        std::ranges::any_of(name, [](unsigned char c) { return c < 32; }))
        return "Use a valid file name";
    std::string upper = name;
    std::ranges::transform(upper, upper.begin(), [](unsigned char c) { return static_cast<char>(std::toupper(c)); });
    if (upper == "CON" || upper == "PRN" || upper == "AUX" || upper == "NUL" ||
        (upper.size() == 4 && (upper.starts_with("COM") || upper.starts_with("LPT")) && upper[3] >= '1' &&
         upper[3] <= '9'))
        return "That name is reserved by Windows";
    if ((!note && name.find('.') != std::string::npos) || name.back() == '.')
        return note ? "No dots at the end of a name" : "No dots in a folder name";
    if (name.front() == ' ' || name.back() == ' ') return "No spaces at the start or end";
    return {};
}
} // namespace

bool IsNoteName(std::string_view name) {
    return name.size() >= 3 && SameWithoutCase(name.substr(name.size() - 3), ".md");
}

std::string AssetNameProblem(AssetNameKind kind, const std::string& name, const std::vector<std::string>& taken) {
    if (name.empty()) return "Enter a name";
    std::string checked = name;
    if (kind == AssetNameKind::Note && IsNoteName(checked)) checked.resize(checked.size() - 3);
    if (checked.empty()) return "Enter a name";
    if (auto problem = kind == AssetNameKind::Graphic ? GraphicProblem(checked)
                                                      : FolderProblem(checked, kind == AssetNameKind::Note);
        !problem.empty())
        return problem;
    for (const auto& existing : taken)
        if (SameWithoutCase(existing, kind == AssetNameKind::Note ? checked + ".md" : checked))
            return existing + " already exists in this folder";
    return {};
}

std::string AssetCommandError(const std::string& output) {
    std::string_view text = output;
    while (!text.empty() && (text.back() == '\n' || text.back() == '\r' || text.back() == ' '))
        text.remove_suffix(1);
    if (const auto newline = text.find_last_of('\n'); newline != std::string_view::npos)
        text.remove_prefix(newline + 1);
    while (!text.empty() && text.front() == ' ')
        text.remove_prefix(1);
    constexpr std::string_view Prefix = "error: ";
    if (text.starts_with(Prefix)) text.remove_prefix(Prefix.size());
    return text.empty() ? std::string("The asset action failed without a message") : std::string(text);
}

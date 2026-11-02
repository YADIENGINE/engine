#include "Editor/Internals/Session/QuickOpen.h"
#include <cctype>
#include <string>
#include "Editor/Internals/Diagnostics/EditorDiagnostics.h"

namespace {
constexpr int WholePrefixScore = 1000;
constexpr int WordStartScore = 100;
constexpr int InLabelScore = 50;
constexpr int InDetailScore = 10;

bool IsWordStart(const std::string& text, std::size_t at) {
    return at == 0 || !std::isalnum(static_cast<unsigned char>(text[at - 1]));
}
int WordScore(const std::string& label, const std::string& detail, std::string_view word) {
    int best = -1;
    for (std::size_t at = label.find(word); at != std::string::npos; at = label.find(word, at + 1))
        best = std::max(best, IsWordStart(label, at) ? WordStartScore : InLabelScore);
    if (best >= 0) return best;
    return detail.find(word) != std::string::npos ? InDetailScore : -1;
}
} // namespace

int QuickOpenScore(std::string_view label, std::string_view detail, std::string_view query) {
    const std::string search = EditorSearchText(std::string(query));
    if (search.empty()) return 0;
    const std::string lowerLabel = EditorSearchText(std::string(label));
    const std::string lowerDetail = EditorSearchText(std::string(detail));
    int score = lowerLabel.starts_with(search) ? WholePrefixScore : 0;
    std::size_t start = 0;
    while (start < search.size()) {
        const auto end = search.find(' ', start);
        const auto word = std::string_view(search).substr(start, end == std::string::npos ? end : end - start);
        if (!word.empty()) {
            const int fit = WordScore(lowerLabel, lowerDetail, word);
            if (fit < 0) return -1;
            score += fit;
        }
        if (end == std::string::npos) break;
        start = end + 1;
    }
    return score;
}

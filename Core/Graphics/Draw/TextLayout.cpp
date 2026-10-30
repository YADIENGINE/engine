#include "Graphics/Draw/TextLayout.h"
#include <algorithm>
#include <stdexcept>
#include <utility>

int FittedTextSize(const char* text, int fontSize, int maxWidth, const TextMeasure& measure) {
    if (maxWidth <= 0) return fontSize;
    if (!measure) throw std::invalid_argument("Text measurement is required");
    const int width = measure(text, fontSize);
    int size = width > maxWidth ? std::max(1, static_cast<int>(static_cast<long long>(fontSize) * maxWidth / width))
                                : fontSize;
    while (size > 1 && measure(text, size) > maxWidth)
        --size;
    return size;
}

TextLayout LayoutText(std::string_view text, const TextLayoutOptions& options, const TextMeasure& measure) {
    if (options.fontSize <= 0 || options.maxWidth < 0 || !measure) {
        throw std::invalid_argument("Invalid text layout options or measurement");
    }
    TextLayout layout{.nominalFontSize = options.fontSize};
    const auto append = [&](std::string line) {
        const int size = options.widthOverflow == TextWidthOverflow::ShrinkToFit
                             ? FittedTextSize(line.c_str(), options.fontSize, options.maxWidth, measure)
                             : options.fontSize;
        const int width = measure(line.c_str(), size);
        layout.lines.push_back({std::move(line), width, size});
    };
    const auto hasRoom = [&] { return options.maxLines == 0 || layout.lines.size() + 1 < options.maxLines; };
    std::string current;
    bool hasLine = false;
    std::size_t start = 0;
    while (start < text.size()) {
        if (text[start] == ' ') {
            ++start;
            continue;
        }
        if (text[start] == '\n' || text[start] == '\r') {
            const bool crlf = text[start] == '\r' && start + 1 < text.size() && text[start + 1] == '\n';
            start += crlf ? 2 : 1;
            if (hasRoom()) {
                append(std::move(current));
                current.clear();
            } else if (options.lineOverflow == TextLineOverflow::Discard) {
                append(std::move(current));
                return layout;
            }
            hasLine = true;
            continue;
        }
        const std::size_t end = std::min(text.find_first_of(" \r\n", start), text.size());
        const auto word = text.substr(start, end - start);
        start = end;
        std::string candidate = current.empty() ? std::string(word) : current + " " + std::string(word);
        const bool appendRemainder = !hasRoom() && options.lineOverflow == TextLineOverflow::AppendToLastLine;
        if (!current.empty() && !appendRemainder && options.maxWidth > 0 &&
            measure(candidate.c_str(), options.fontSize) > options.maxWidth) {
            append(std::move(current));
            if (options.maxLines > 0 && layout.lines.size() >= options.maxLines) return layout;
            current = std::string(word);
        } else {
            current = std::move(candidate);
        }
        hasLine = true;
    }
    if (hasLine) append(std::move(current));
    return layout;
}

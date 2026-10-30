#pragma once
#include <cstddef>
#include <functional>
#include <string>
#include <string_view>
#include <vector>

enum class TextWidthOverflow { Allow, ShrinkToFit };
enum class TextLineOverflow { Discard, AppendToLastLine };

struct TextLayoutOptions {
    int fontSize = 16;
    int maxWidth = 0;         // zero leaves width unrestricted
    std::size_t maxLines = 0; // zero allows unlimited lines
    TextWidthOverflow widthOverflow = TextWidthOverflow::Allow;
    TextLineOverflow lineOverflow = TextLineOverflow::Discard;
};

struct TextLayoutLine {
    std::string text;
    int width = 0;
    int fontSize = 0;
};

// Owns its text and measurements; does not retain the font or measurement callback.
struct TextLayout {
    int nominalFontSize = 0;
    std::vector<TextLayoutLine> lines;
};

using TextMeasure = std::function<int(const char*, int)>;

[[nodiscard]] int FittedTextSize(const char* text, int fontSize, int maxWidth, const TextMeasure& measure);
[[nodiscard]] TextLayout LayoutText(std::string_view text, const TextLayoutOptions& options,
                                    const TextMeasure& measure);

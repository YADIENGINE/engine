#pragma once
#include <string_view>

// How well `query` fits an entry of the quick open list; negative when it does not. Every space-separated word of the
// query must appear in the label or the detail, in any order, ignoring case. A label that starts with the query comes
// first, then words at the start of a label's word, then anywhere in the label, then only in the detail.
[[nodiscard]] int QuickOpenScore(std::string_view label, std::string_view detail, std::string_view query);

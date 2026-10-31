#pragma once
#include <string_view>

// Opens a URL in the user's default external application.
void OpenExternalUrl(std::string_view url);
// Copies UTF-8 text to the system clipboard.
void CopyToClipboard(std::string_view text);

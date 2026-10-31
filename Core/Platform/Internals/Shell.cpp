#include "Platform/Shell.h"
#include <raylib.h>
#include <string>

void OpenExternalUrl(std::string_view url) { OpenURL(std::string(url).c_str()); }
void CopyToClipboard(std::string_view text) { SetClipboardText(std::string(text).c_str()); }

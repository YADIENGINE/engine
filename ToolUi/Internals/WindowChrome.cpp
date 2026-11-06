#include "ToolUi/Internals/WindowChrome.h"
#define WIN32_LEAN_AND_MEAN
#define NOMINMAX
#include <windows.h>
#include <dwmapi.h>

namespace {
// Numeric ids: the attributes are newer than some SDKs' headers.
constexpr DWORD UseImmersiveDarkMode = 20;
constexpr DWORD BorderColor = 34;
constexpr DWORD CaptionColor = 35;
constexpr DWORD TextColor = 36;

COLORREF ToColorRef(ImVec4 color) {
    const auto channel = [](float value) { return static_cast<BYTE>(value * 255.0f + 0.5f); };
    return RGB(channel(color.x), channel(color.y), channel(color.z));
}
} // namespace

void ApplyToolWindowChrome(void* windowHandle, ImVec4 caption, ImVec4 text) {
    const auto window = static_cast<HWND>(windowHandle);
    if (!window) return;
    const BOOL dark = caption.x * 0.2126f + caption.y * 0.7152f + caption.z * 0.0722f < 0.5f;
    (void)DwmSetWindowAttribute(window, UseImmersiveDarkMode, &dark, sizeof(dark));
    const COLORREF captionColor = ToColorRef(caption);
    const COLORREF textColor = ToColorRef(text);
    (void)DwmSetWindowAttribute(window, CaptionColor, &captionColor, sizeof(captionColor));
    (void)DwmSetWindowAttribute(window, BorderColor, &captionColor, sizeof(captionColor));
    (void)DwmSetWindowAttribute(window, TextColor, &textColor, sizeof(textColor));
}

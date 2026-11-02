#pragma once
#include <imgui.h>

// Colors the native title bar to match the tool theme (dark caption; exact color on Windows 11). Best effort: older
// Windows versions ignore what they do not support. `windowHandle` is the HWND.
void ApplyToolWindowChrome(void* windowHandle, ImVec4 caption, ImVec4 text);

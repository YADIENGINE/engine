#include <algorithm>
#include <array>
#include <cmath>
#include <string>
#include <string_view>
#include "Editor/Api/GraphicAsset.h"
#include "Graphics/Input/KeyPrompts.h"

namespace {
constexpr float PressRate = 3.0f; // The preview keeps pressing the key, to show 'pressed'

struct PromptItem {
    std::string_view name;
    std::string_view tags;
};

constexpr std::string_view Letter = "key keyboard letter hint";
constexpr std::string_view Digit = "key keyboard number digit hint";
constexpr std::string_view Function = "key keyboard function hint";
constexpr std::string_view Named = "key keyboard hint";
constexpr std::string_view Direction = "key keyboard direction arrow hint";
constexpr std::string_view Pointer = "mouse pointer hint";

// clang-format off
// In KeyPrompt's order. Each name is also the enum token used by Copy usage.
constexpr auto PromptItems = std::to_array<PromptItem>({
    {"A", Letter}, {"B", Letter}, {"C", Letter}, {"D", Letter}, {"E", Letter}, {"F", Letter}, {"G", Letter},
    {"H", Letter}, {"I", Letter}, {"J", Letter}, {"K", Letter}, {"L", Letter}, {"M", Letter}, {"N", Letter},
    {"O", Letter}, {"P", Letter}, {"Q", Letter}, {"R", Letter}, {"S", Letter}, {"T", Letter}, {"U", Letter},
    {"V", Letter}, {"W", Letter}, {"X", Letter}, {"Y", Letter}, {"Z", Letter},
    {"Num0", Digit}, {"Num1", Digit}, {"Num2", Digit}, {"Num3", Digit}, {"Num4", Digit},
    {"Num5", Digit}, {"Num6", Digit}, {"Num7", Digit}, {"Num8", Digit}, {"Num9", Digit},
    {"F1", Function}, {"F2", Function}, {"F3", Function}, {"F4", Function}, {"F5", Function}, {"F6", Function},
    {"F7", Function}, {"F8", Function}, {"F9", Function}, {"F10", Function}, {"F11", Function}, {"F12", Function},
    {"Space", Named}, {"Enter", Named}, {"Escape", Named}, {"Tab", Named}, {"Shift", Named}, {"Ctrl", Named},
    {"Alt", Named}, {"Backspace", Named}, {"Delete", Named},
    {"Up", Direction}, {"Down", Direction}, {"Left", Direction}, {"Right", Direction},
    {"Wasd", "key keyboard direction movement set hint"}, {"ArrowKeys", "key keyboard direction movement set hint"},
    {"Mouse", Pointer}, {"MouseLeft", "mouse button click hint"}, {"MouseRight", "mouse button click hint"},
    {"MouseMiddle", "mouse button click wheel hint"}, {"MouseWheel", "mouse scroll zoom hint"},
    {"MouseMove", "mouse look aim pointer hint"},
});
// clang-format on
static_assert(PromptItems.size() == KeyPromptCount);

GraphicAsset MakeKeyPrompts() {
    GraphicAsset asset;
    for (const PromptItem& item : PromptItems)
        asset.items.push_back({std::string(item.name), std::string(item.tags)});
    asset.usage = "DrawKeyPrompt(paint, box, KeyPrompt::{item});";
    asset.draw = [](Painter& paint, const FloatRect& box, std::size_t item, float time) {
        DrawKeyPrompt(paint, box, static_cast<KeyPrompt>(item), std::max(0.0f, std::sin(time * PressRate)));
    };
    return asset;
}
[[maybe_unused]] const bool Registered = RegisterGraphicAsset(&MakeKeyPrompts);
} // namespace

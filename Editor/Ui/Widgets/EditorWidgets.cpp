#include "Editor/Ui/Widgets/EditorWidgets.h"
#include <extras/IconsFontAwesome6.h>
#include <imgui.h>
#include <algorithm>
#include "Editor/Ui/Style/EditorTheme.h"
#include "Editor/Ui/Widgets/PropertyWidgets.h"
#include "ToolUi/ToolWidgets.h"

bool EditorIconButton(const char* id, EditorGlyph glyph, const char* tooltip, bool selected) {
    return ToolIconButton(id, EditorGlyphText(glyph), tooltip, selected);
}
void EditorSearchInput(const char* id, const char* hint, char* text, std::size_t capacity, float width, bool& focus) {
    const bool hasText = text[0] != '\0';
    const float clearWidth = hasText ? ImGui::GetFrameHeight() + ImGui::GetStyle().ItemSpacing.x : 0;
    ImGui::SetNextItemWidth(std::max(1.0f, width - clearWidth));
    if (focus) {
        ImGui::SetKeyboardFocusHere();
        focus = false;
    }
    ImGui::InputTextWithHint(id, hint, text, capacity);
    if (hasText) {
        ImGui::SameLine();
        ImGui::PushID(id);
        if (ToolIconButton("clear-search", ICON_FA_XMARK, "Clear search")) {
            text[0] = '\0';
            focus = true;
        }
        ImGui::PopID();
    }
}
int EditorChoiceColumns(const SceneParameter& parameter) { return PropertyChoiceColumns(ScenePropertySpec(parameter)); }
bool EditorParameterWidget(const SceneParameter& parameter, SceneValue& value) {
    auto property = ScenePropertyValue(parameter, value);
    if (!PropertyWidget(ScenePropertySpec(parameter), property)) return false;
    value = SceneValueOf(parameter, property);
    return true;
}

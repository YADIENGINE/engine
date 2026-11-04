#include "Editor/Ui/Widgets/EditorWidgets.h"
#include "Editor/Ui/Style/EditorTheme.h"
#include "Editor/Ui/Widgets/PropertyWidgets.h"
#include "ToolUi/ToolWidgets.h"

bool EditorIconButton(const char* id, EditorGlyph glyph, const char* tooltip, bool selected) {
    return ToolIconButton(id, EditorGlyphText(glyph), tooltip, selected);
}
int EditorChoiceColumns(const SceneParameter& parameter) { return PropertyChoiceColumns(ScenePropertySpec(parameter)); }
bool EditorParameterWidget(const SceneParameter& parameter, SceneValue& value) {
    auto property = ScenePropertyValue(parameter, value);
    if (!PropertyWidget(ScenePropertySpec(parameter), property)) return false;
    value = SceneValueOf(parameter, property);
    return true;
}

#pragma once
#include <functional>
#include <span>
#include <string>
#include "Editor/Api/SceneParameter.h"
#include "Editor/Ui/Widgets/Properties.h"

struct SceneReadout;

// These scopes affect only Inspector content, never docking, toolbars or other panels.
void BeginInspectorStyle();
void EndInspectorStyle();
[[nodiscard]] bool InspectorSection(const char* label, bool forceOpen = false, bool defaultOpen = true);
[[nodiscard]] bool BeginInspectorProperties(const char* id, float labelWidth, bool readOnly = false);
void EndInspectorProperties();
// True when a choice-button property needs its own full-width block instead of a row of the properties table: too
// many choices for one row, or the row is too narrow. Such a field is drawn outside BeginInspectorProperties. `width`
// is the panel's content width, measured outside any table.
[[nodiscard]] bool InspectorChoiceIsBlock(const PropertySpec& spec, float labelWidth, float width);
[[nodiscard]] bool InspectorChoiceIsBlock(const SceneParameter& parameter, float labelWidth, float width);
// Short enough to sit on one row, so its label counts toward the shared label column.
[[nodiscard]] bool InspectorChoiceFitsRow(const PropertySpec& spec);
[[nodiscard]] bool InspectorChoiceFitsRow(const SceneParameter& parameter);

// One property row: label, value and a reset button. The owner decides what reset means (a scene parameter's default,
// a tuning field's committed value) and what an edit does.
struct InspectorRow {
    std::string label;
    std::string tooltip;   // Hover text of the label and the value
    bool modified = false; // Marks the row and enables reset
    bool pending = false;  // Waiting for the scene to accept the value
    std::string resetTooltip = "Default: restore this parameter's default value.";
};
struct InspectorEdit {
    bool changed = false; // The value changed this frame, also each step of a drag
    bool reset = false;   // The reset button was pressed
    bool context = false; // Right-click on the label or value
};
[[nodiscard]] InspectorEdit InspectorPropertyField(const InspectorRow& row, const PropertySpec& spec,
                                                   PropertyValue& value);
// A scene parameter's row: reset restores its default; `applyHint` says when the value applies.
[[nodiscard]] bool InspectorParameterField(const SceneParameter& parameter, SceneValue& value, bool pending,
                                           const char* applyHint = "", bool* contextRequested = nullptr);
void InspectorReadOnlyField(const SceneReadout& value);
void InspectorSummary(const char* title, const char* description);
void InspectorPrimaryAction(const SceneAction& action, std::span<const std::string> disabled,
                            const std::function<void(const std::string&)>& execute);
void InspectorActionGroup(std::span<const SceneAction> actions, std::span<const std::string> disabled,
                          const std::function<void(const std::string&)>& execute, bool includePrimary = true);

#pragma once
#include <functional>
#include <span>
#include "Editor/Api/SceneParameter.h"

struct SceneReadout;

// These scopes affect only Inspector content, never docking, toolbars or other panels.
void BeginInspectorStyle();
void EndInspectorStyle();
[[nodiscard]] bool InspectorSection(const char* label, bool forceOpen = false, bool defaultOpen = true);
[[nodiscard]] bool BeginInspectorProperties(const char* id, float labelWidth, bool readOnly = false);
void EndInspectorProperties();
// True when a choice-button parameter needs its own full-width block instead of a row of the properties table: too
// many choices for one row, or the row is too narrow. Such a field is drawn outside BeginInspectorProperties. `width`
// is the panel's content width, measured outside any table.
[[nodiscard]] bool InspectorChoiceIsBlock(const SceneParameter& parameter, float labelWidth, float width);
// Short enough to sit on one row, so its label counts toward the shared label column.
[[nodiscard]] bool InspectorChoiceFitsRow(const SceneParameter& parameter);
[[nodiscard]] bool InspectorParameterField(const SceneParameter& parameter, SceneValue& value, bool pending,
                                           const char* applyHint = "", bool* contextRequested = nullptr);
void InspectorReadOnlyField(const SceneReadout& value);
void InspectorSummary(const char* title, const char* description);
void InspectorPrimaryAction(const SceneAction& action, std::span<const std::string> disabled,
                            const std::function<void(const std::string&)>& execute);
void InspectorActionGroup(std::span<const SceneAction> actions, std::span<const std::string> disabled,
                          const std::function<void(const std::string&)>& execute, bool includePrimary = true);

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
[[nodiscard]] bool InspectorParameterField(const SceneParameter& parameter, SceneValue& value, bool pending,
                                           const char* applyHint = "", bool* contextRequested = nullptr);
void InspectorReadOnlyField(const SceneReadout& value);
void InspectorSummary(const char* title, const char* description);
void InspectorPrimaryAction(const SceneAction& action, std::span<const std::string> disabled,
                            const std::function<void(const std::string&)>& execute);
void InspectorActionGroup(std::span<const SceneAction> actions, std::span<const std::string> disabled,
                          const std::function<void(const std::string&)>& execute, bool includePrimary = true);

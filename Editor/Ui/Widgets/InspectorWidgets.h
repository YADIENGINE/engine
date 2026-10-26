#pragma once
#include <functional>
#include <span>
#include "Editor/Api/PreviewParameter.h"

struct PreviewReadout;

// These scopes affect only Inspector content, never docking, toolbars or other panels.
void BeginInspectorStyle();
void EndInspectorStyle();
[[nodiscard]] bool InspectorSection(const char* label);
[[nodiscard]] bool BeginInspectorProperties(const char* id);
void EndInspectorProperties();
[[nodiscard]] bool InspectorParameterField(const PreviewParameter& parameter, PreviewValue& value, bool pending);
void InspectorReadOnlyField(const PreviewReadout& value);
void InspectorActionGroup(std::span<const PreviewAction> actions, std::span<const std::string> disabled,
                          const std::function<void(const std::string&)>& execute);

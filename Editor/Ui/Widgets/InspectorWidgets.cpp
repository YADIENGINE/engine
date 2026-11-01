#include "Editor/Ui/Widgets/InspectorWidgets.h"
#include <imgui.h>
#include <algorithm>
#include <cfloat>
#include <cmath>
#include <string>
#include "Editor/Api/SceneInspection.h"
#include "Editor/Ui/Style/EditorTheme.h"
#include "Editor/Ui/Widgets/EditorWidgets.h"
#include "ToolUi/ToolTheme.h"
#include "ToolUi/ToolWidgets.h"

namespace {
constexpr float BaseFontSize = 14.0f;
constexpr float MetadataFontSize = 11.0f;
constexpr float ResetButtonSize = 20.0f;
constexpr float DropdownPaddingY = 4.0f;
constexpr float CheckboxPaddingY = 2.0f;
constexpr float MaximumLabelWidth = 140.0f;
constexpr float MinimumControlWidth = 96.0f;
constexpr float LabelWidthFraction = 0.36f;

float InspectorScale() { return ImGui::GetFontSize() / BaseFontSize; }
ImVec4 AccentSurface(ImGuiCol surface, float amount, ToolTone tone = ToolTone::Accent) {
    const auto base = ImGui::GetStyleColorVec4(surface);
    const auto accent = ToolToneColor(tone);
    return {std::lerp(base.x, accent.x, amount), std::lerp(base.y, accent.y, amount),
            std::lerp(base.z, accent.z, amount), base.w};
}
std::string InspectorDisplayText(const char* text, float width) {
    std::string display = text;
    if (ImGui::CalcTextSize(text).x > width) {
        const float ellipsisWidth = ImGui::CalcTextSize("...").x;
        if (ellipsisWidth > width) return {};
        while (!display.empty() && ImGui::CalcTextSize(display.c_str()).x + ellipsisWidth > width) {
            auto end = display.size() - 1;
            while (end > 0 && (static_cast<unsigned char>(display[end]) & 0xc0) == 0x80)
                --end;
            display.resize(end);
        }
        display += "...";
    }
    return display;
}
void InspectorText(const char* text, float width, bool rightAligned = false) {
    width = std::max(1.0f, width);
    const auto position = ImGui::GetCursorScreenPos();
    const auto display = InspectorDisplayText(text, width);
    const float offset = rightAligned ? std::max(0.0f, width - ImGui::CalcTextSize(display.c_str()).x) : 0.0f;
    ImGui::Dummy({width, ImGui::GetTextLineHeight()});
    const ImVec4 clip{position.x, position.y, position.x + width, position.y + ImGui::GetTextLineHeight()};
    ImGui::GetWindowDrawList()->AddText(ImGui::GetFont(), ImGui::GetFontSize(), {position.x + offset, position.y},
                                        ImGui::GetColorU32(ImGuiCol_Text), display.c_str(), nullptr, 0, &clip);
    if (display != text) ToolTooltip(text);
}
void InspectorLabel(const char* label, const char* description, bool modified, float width) {
    const float scale = InspectorScale();
    const float markerWidth = std::min(8 * scale, std::max(0.0f, width - 1));
    const auto marker = ImGui::GetCursorScreenPos();
    if (modified && markerWidth >= 6 * scale)
        ImGui::GetWindowDrawList()->AddCircleFilled(
            {marker.x + 2 * scale, marker.y + ImGui::GetTextLineHeight() * 0.5f}, 2 * scale,
            ImGui::GetColorU32(ToolToneColor(ToolTone::Warning)));
    ImGui::SetCursorPosX(ImGui::GetCursorPosX() + markerWidth);
    InspectorText(label, std::max(1.0f, width - markerWidth));
    if (*description) ToolTooltip(description);
}
bool InspectorResetButton(bool changed, bool pending, bool inlineRow) {
    const float size = ResetButtonSize * InspectorScale();
    const float offset = inlineRow ? std::max(0.0f, (ImGui::GetFrameHeight() - size) * 0.5f) : 0.0f;
    ImGui::SetCursorPosY(ImGui::GetCursorPosY() + offset);
    ImGui::PushFont(ToolFont(ToolFontRole::Ui), ImGui::GetStyle().FontSizeBase * MetadataFontSize / BaseFontSize);
    ImGui::PushStyleVar(ImGuiStyleVar_FramePadding, {0, 0});
    ImGui::PushStyleColor(ImGuiCol_Text, ImGui::GetStyleColorVec4(ImGuiCol_TextDisabled));
    if (!changed) ImGui::PushStyleColor(ImGuiCol_Button, {0, 0, 0, 0});
    ImGui::BeginDisabled(pending);
    const bool reset = ImGui::Button("##default", {size, size}) && changed;
    if (changed || ImGui::IsItemHovered(ImGuiHoveredFlags_AllowWhenDisabled) || ImGui::IsItemFocused()) {
        const auto position = ImGui::GetItemRectMin();
        const char* glyph = EditorGlyphText(EditorGlyph::Reset);
        const auto textSize = ImGui::CalcTextSize(glyph);
        ImGui::GetWindowDrawList()->AddText(
            {position.x + (size - textSize.x) * 0.5f, position.y + (size - textSize.y) * 0.5f},
            ImGui::GetColorU32(ImGuiCol_Text), glyph);
    }
    ImGui::EndDisabled();
    if (!changed) ImGui::PopStyleColor();
    ToolTooltip("Default: restore this parameter's default value.");
    ImGui::PopStyleColor();
    ImGui::PopStyleVar();
    ImGui::PopFont();
    return reset;
}
void InspectorAction(const SceneAction& action, std::span<const std::string> disabled,
                     const std::function<void(const std::string&)>& execute) {
    ImGui::PushID(action.id.c_str());
    ImGui::BeginDisabled(std::ranges::find(disabled, action.id) != disabled.end());
    if (action.primary) {
        ImGui::PushStyleColor(ImGuiCol_Button, AccentSurface(ImGuiCol_FrameBg, 0.18f));
        ImGui::PushStyleColor(ImGuiCol_ButtonHovered, AccentSurface(ImGuiCol_FrameBg, 0.27f));
        ImGui::PushStyleColor(ImGuiCol_ButtonActive, AccentSurface(ImGuiCol_FrameBg, 0.36f));
    }
    if (ToolSingleLineButton(action.label.c_str())) execute(action.id);
    const std::string tooltip = action.label + (action.description.empty() ? "" : "\n" + action.description);
    ToolTooltip(tooltip.c_str());
    if (action.primary) ImGui::PopStyleColor(3);
    ImGui::EndDisabled();
    ImGui::PopID();
}
} // namespace
void BeginInspectorStyle() {
    ImGui::PushFont(ToolFont(ToolFontRole::Ui), 0);
    const float scale = InspectorScale();
    ImGui::PushStyleVar(ImGuiStyleVar_FramePadding, {8 * scale, 5 * scale});
    ImGui::PushStyleVar(ImGuiStyleVar_ItemSpacing, {6 * scale, 4 * scale});
    ImGui::PushStyleVar(ImGuiStyleVar_CellPadding, {4 * scale, 2 * scale});
    ImGui::PushStyleVar(ImGuiStyleVar_FrameRounding, 4 * scale);
    ImGui::PushStyleVar(ImGuiStyleVar_GrabRounding, 3 * scale);
    ImGui::PushStyleVar(ImGuiStyleVar_FrameBorderSize, 0);
    ImGui::PushStyleColor(ImGuiCol_Button, ImGui::GetStyleColorVec4(ImGuiCol_FrameBg));
    ImGui::PushStyleColor(ImGuiCol_ButtonHovered, ImGui::GetStyleColorVec4(ImGuiCol_FrameBgHovered));
    ImGui::PushStyleColor(ImGuiCol_ButtonActive, ImGui::GetStyleColorVec4(ImGuiCol_FrameBgActive));
}
void EndInspectorStyle() {
    ImGui::PopStyleColor(3);
    ImGui::PopStyleVar(6);
    ImGui::PopFont();
}
bool InspectorSection(const char* label, bool forceOpen, bool defaultOpen) {
    return ToolSectionHeader(label, defaultOpen, forceOpen);
}
bool BeginInspectorProperties(const char* id, float labelWidth, bool readOnly) {
    constexpr auto flags =
        ImGuiTableFlags_SizingFixedFit | ImGuiTableFlags_NoSavedSettings | ImGuiTableFlags_NoPadOuterX;
    const float width = ImGui::GetContentRegionAvail().x;
    const float scale = InspectorScale();
    labelWidth = std::min({labelWidth, MaximumLabelWidth * scale, width * LabelWidthFraction});
    const float resetWidth = readOnly ? 0 : ResetButtonSize * scale;
    const bool inlineRow =
        width >= labelWidth + resetWidth + MinimumControlWidth * scale + ImGui::GetStyle().CellPadding.x * 6;
    const std::string tableId = std::string("##properties.") + id;
    if (!ImGui::BeginTable(tableId.c_str(), inlineRow ? (readOnly ? 2 : 3) : 1, flags)) return false;
    if (inlineRow) {
        ImGui::TableSetupColumn("Label", ImGuiTableColumnFlags_WidthFixed, std::max(1.0f, labelWidth));
        ImGui::TableSetupColumn("Value", ImGuiTableColumnFlags_WidthStretch);
        if (!readOnly) ImGui::TableSetupColumn("Default", ImGuiTableColumnFlags_WidthFixed, resetWidth);
    } else {
        ImGui::TableSetupColumn("Property", ImGuiTableColumnFlags_WidthStretch);
    }
    return true;
}
void EndInspectorProperties() { ImGui::EndTable(); }
bool InspectorParameterField(const SceneParameter& parameter, SceneValue& value, bool pending, const char* applyHint,
                             bool* contextRequested) {
    const auto context = [&] {
        if (contextRequested && ImGui::IsItemHovered(ImGuiHoveredFlags_AllowWhenDisabled) &&
            ImGui::IsMouseReleased(ImGuiMouseButton_Right))
            *contextRequested = true;
    };
    const bool modified = value != parameter.initial;
    const std::string tooltip = parameter.label + (modified ? "\nModified from default." : "") +
                                (parameter.description.empty() ? "" : "\n" + parameter.description);
    const std::string labelTooltip = tooltip + (*applyHint ? "\n" + std::string(applyHint) : "");
    const bool compact = parameter.kind == SceneParameterKind::Boolean ||
                         (parameter.kind == SceneParameterKind::Choice && !parameter.choiceButtons);
    if (compact) {
        const float padding = parameter.kind == SceneParameterKind::Boolean ? CheckboxPaddingY : DropdownPaddingY;
        ImGui::PushStyleVar(ImGuiStyleVar_FramePadding, {ImGui::GetStyle().FramePadding.x, padding * InspectorScale()});
    }
    // Choice grids are standalone full-width blocks; ordinary fields share their group's table.
    const bool choiceGrid = parameter.kind == SceneParameterKind::Choice && parameter.choiceButtons;
    const bool inlineRow = !choiceGrid && ImGui::TableGetColumnCount() > 1;
    const float resetSize = ResetButtonSize * InspectorScale();
    const float gap = ImGui::GetStyle().ItemSpacing.x;
    const bool inlineCheckbox =
        !inlineRow && parameter.kind == SceneParameterKind::Boolean &&
        ImGui::GetContentRegionAvail().x >= resetSize + ImGui::GetFrameHeight() + gap * 2 + ImGui::GetFontSize();
    if (choiceGrid) {
        ImGui::BeginGroup();
    } else {
        ImGui::TableNextRow();
        ImGui::TableSetColumnIndex(0);
    }
    if (inlineRow) {
        ImGui::SetCursorPosY(ImGui::GetCursorPosY() + ImGui::GetStyle().FramePadding.y);
        InspectorLabel(parameter.label.c_str(), labelTooltip.c_str(), modified, ImGui::GetContentRegionAvail().x);
        context();
        ImGui::TableSetColumnIndex(2);
    } else {
        const float checkboxWidth = inlineCheckbox ? ImGui::GetFrameHeight() + gap : 0;
        const float labelWidth = std::max(1.0f, ImGui::GetContentRegionAvail().x - resetSize - gap - checkboxWidth);
        ImGui::AlignTextToFramePadding();
        InspectorLabel(parameter.label.c_str(), labelTooltip.c_str(), modified, labelWidth);
        context();
        ImGui::SameLine(0, gap);
    }
    bool edited = false;
    if (!inlineCheckbox) edited = InspectorResetButton(modified, pending, inlineRow);
    if (edited) value = parameter.initial;
    if (inlineRow) ImGui::TableSetColumnIndex(1);
    ImGui::SetNextItemWidth(-FLT_MIN);
    const bool technical = parameter.kind == SceneParameterKind::Integer ||
                           parameter.kind == SceneParameterKind::Float || parameter.kind == SceneParameterKind::Color;
    if (technical) ImGui::PushFont(ToolFont(ToolFontRole::Technical), 0);
    if (modified) {
        ImGui::PushStyleVar(ImGuiStyleVar_FrameBorderSize, InspectorScale());
        ImGui::PushStyleColor(ImGuiCol_Border, AccentSurface(ImGuiCol_Border, 0.35f, ToolTone::Warning));
    }
    const bool modifiedNumber =
        modified && (parameter.kind == SceneParameterKind::Integer || parameter.kind == SceneParameterKind::Float);
    if (modifiedNumber) ImGui::PushStyleColor(ImGuiCol_Text, ToolToneColor(ToolTone::Warning));
    edited = EditorParameterWidget(parameter, value) || edited;
    context();
    if (modifiedNumber) ImGui::PopStyleColor();
    ToolTooltip(tooltip.c_str());
    if (modified) {
        ImGui::PopStyleColor();
        ImGui::PopStyleVar();
    }
    if (technical) ImGui::PopFont();
    if (inlineCheckbox) {
        ImGui::SameLine(0, gap);
        if (InspectorResetButton(modified, pending, true)) {
            value = parameter.initial;
            edited = true;
        }
    }
    if (pending) {
        ToolStatusBadge("Pending", EditorPhaseColor(EditorPhase::Paused));
        ToolTooltip("Waiting for the scene to accept this value.");
    }
    if (choiceGrid) {
        ImGui::EndGroup();
        context();
    }
    if (compact) ImGui::PopStyleVar();
    return edited;
}
void InspectorReadOnlyField(const SceneReadout& value) {
    ImGui::TableNextRow();
    ImGui::TableSetColumnIndex(0);
    InspectorLabel(value.label.c_str(), "", false, ImGui::GetContentRegionAvail().x);
    if (ImGui::TableGetColumnCount() > 1) ImGui::TableSetColumnIndex(1);
    switch (value.kind) {
    case SceneReadoutKind::Number:
        ImGui::PushFont(ToolFont(ToolFontRole::Technical), 0);
        InspectorText(value.value.c_str(), ImGui::GetContentRegionAvail().x, true);
        ImGui::PopFont();
        break;
    case SceneReadoutKind::Status:
        if (ImGui::CalcTextSize(value.value.c_str()).x + ImGui::GetStyle().FramePadding.x * 2 <=
            ImGui::GetContentRegionAvail().x) {
            ToolStatusBadge(value.value.c_str(), ToolToneColor(ToolTone::Information));
            break;
        }
        [[fallthrough]];
    case SceneReadoutKind::Text:
        ImGui::PushTextWrapPos(0);
        ImGui::TextUnformatted(value.value.c_str());
        ImGui::PopTextWrapPos();
        break;
    }
}
void InspectorSummary(const char* title, const char* text) {
    const float helpWidth = *text ? ImGui::GetFrameHeight() : 0;
    const float gap = *text ? ImGui::GetStyle().ItemSpacing.x : 0;
    const float titleSize = ImGui::GetStyle().FontSizeBase * 1.1f;
    const float width = std::max(1.0f, ImGui::GetContentRegionAvail().x - helpWidth - gap);
    ImGui::PushFont(ToolFont(ToolFontRole::Ui), titleSize);
    InspectorText(title, width);
    ImGui::PopFont();
    if (*text) {
        ImGui::SameLine(0, gap);
        if (ImGui::Button("?##description", {helpWidth, helpWidth})) ImGui::OpenPopup("##description");
        ToolTooltip(text);
        if (ImGui::BeginPopup("##description")) {
            ImGui::PushTextWrapPos(ImGui::GetFontSize() * 28);
            ImGui::TextUnformatted(text);
            ImGui::PopTextWrapPos();
            ImGui::EndPopup();
        }
    }
}
void InspectorPrimaryAction(const SceneAction& action, std::span<const std::string> disabled,
                            const std::function<void(const std::string&)>& execute) {
    InspectorAction(action, disabled, execute);
}
void InspectorActionGroup(std::span<const SceneAction> actions, std::span<const std::string> disabled,
                          const std::function<void(const std::string&)>& execute, bool includePrimary) {
    const auto primary = std::ranges::find(actions, true, &SceneAction::primary);
    if (includePrimary && primary != actions.end()) InspectorAction(*primary, disabled, execute);
    if (actions.empty() || (actions.size() == 1 && primary != actions.end())) return;
    float minimumWidth = 0;
    for (const auto& action : actions)
        if (!action.primary) minimumWidth = std::max(minimumWidth, ImGui::CalcTextSize(action.label.c_str()).x);
    minimumWidth += ImGui::GetStyle().FramePadding.x * 2;
    const int columns = ImGui::GetContentRegionAvail().x >= minimumWidth * 2 + ImGui::GetStyle().ItemSpacing.x ? 2 : 1;
    ImGui::PushStyleVar(ImGuiStyleVar_CellPadding, {ImGui::GetStyle().ItemSpacing.x * 0.5f, 2 * InspectorScale()});
    if (ImGui::BeginTable("##actions", columns,
                          ImGuiTableFlags_SizingStretchSame | ImGuiTableFlags_NoPadOuterX |
                              ImGuiTableFlags_NoSavedSettings)) {
        for (const auto& action : actions) {
            if (action.primary) continue;
            ImGui::TableNextColumn();
            InspectorAction(action, disabled, execute);
        }
        ImGui::EndTable();
    }
    ImGui::PopStyleVar();
}

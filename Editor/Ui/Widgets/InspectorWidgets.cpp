#include "Editor/Ui/Widgets/InspectorWidgets.h"
#include <imgui.h>
#include <algorithm>
#include <cfloat>
#include <cmath>
#include <string>
#include "Editor/Api/PreviewInspection.h"
#include "Editor/Ui/Style/EditorTheme.h"
#include "Editor/Ui/Widgets/EditorWidgets.h"

namespace {
constexpr float BaseFontSize = 14.0f;
constexpr float MetadataFontSize = 11.0f;
constexpr float ResetButtonSize = 20.0f;
constexpr float DropdownPaddingY = 4.0f;
constexpr float CheckboxPaddingY = 2.0f;
constexpr float MaximumLabelWidth = 140.0f;
constexpr float MinimumControlWidth = 140.0f;
constexpr float DescriptionLines = 2.0f;

float InspectorScale() { return ImGui::GetFontSize() / BaseFontSize; }
ImVec4 AccentSurface(ImGuiCol surface, float amount) {
    const auto base = ImGui::GetStyleColorVec4(surface);
    const auto accent = ImGui::GetStyleColorVec4(ImGuiCol_CheckMark);
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
    if (display != text) EditorTooltip(text);
}
void InspectorLabel(const char* label, const char* description, bool modified, float width) {
    const float scale = InspectorScale();
    const float markerWidth = std::min(8 * scale, std::max(0.0f, width - 1));
    const auto marker = ImGui::GetCursorScreenPos();
    if (modified && markerWidth >= 6 * scale)
        ImGui::GetWindowDrawList()->AddCircleFilled(
            {marker.x + 2 * scale, marker.y + ImGui::GetTextLineHeight() * 0.5f}, 2 * scale,
            ImGui::GetColorU32(ImGuiCol_CheckMark));
    ImGui::SetCursorPosX(ImGui::GetCursorPosX() + markerWidth);
    InspectorText(label, std::max(1.0f, width - markerWidth));
    if (*description) EditorTooltip(description);
}
const char* ApplyLabel(PreviewApplyMode apply) {
    switch (apply) {
    case PreviewApplyMode::Immediate : return "";
    case PreviewApplyMode::NextAction: return "Next action";
    case PreviewApplyMode::OnReset   : return "Reset to apply";
    }
    return "";
}
bool InspectorResetButton(bool changed, bool pending, bool inlineRow) {
    const float size = ResetButtonSize * InspectorScale();
    const float offset = inlineRow ? std::max(0.0f, (ImGui::GetFrameHeight() - size) * 0.5f) : 0.0f;
    ImGui::SetCursorPosY(ImGui::GetCursorPosY() + offset);
    ImGui::PushFont(EditorFont(EditorFontRole::Ui), ImGui::GetStyle().FontSizeBase * MetadataFontSize / BaseFontSize);
    ImGui::PushStyleVar(ImGuiStyleVar_FramePadding, {0, 0});
    ImGui::PushStyleColor(ImGuiCol_Text, ImGui::GetStyleColorVec4(ImGuiCol_TextDisabled));
    ImGui::BeginDisabled(!changed || pending);
    const bool reset = ImGui::Button(EditorGlyphText(EditorGlyph::Reset), {size, size});
    ImGui::EndDisabled();
    EditorTooltip("Default: restore this parameter's default value.");
    ImGui::PopStyleColor();
    ImGui::PopStyleVar();
    ImGui::PopFont();
    return reset;
}
void InspectorPropertyMetadata(PreviewApplyMode apply) {
    const char* label = ApplyLabel(apply);
    if (!*label) return;
    ImGui::PushFont(EditorFont(EditorFontRole::Ui), ImGui::GetStyle().FontSizeBase * MetadataFontSize / BaseFontSize);
    ImGui::PushStyleColor(ImGuiCol_Text, ImGui::GetStyleColorVec4(ImGuiCol_TextDisabled));
    InspectorText(label, ImGui::GetContentRegionAvail().x);
    ImGui::PopStyleColor();
    ImGui::PopFont();
}
void InspectorAction(const PreviewAction& action, std::span<const std::string> disabled,
                     const std::function<void(const std::string&)>& execute) {
    ImGui::PushID(action.id.c_str());
    ImGui::BeginDisabled(std::ranges::find(disabled, action.id) != disabled.end());
    if (action.primary) {
        ImGui::PushStyleColor(ImGuiCol_Button, AccentSurface(ImGuiCol_FrameBg, 0.18f));
        ImGui::PushStyleColor(ImGuiCol_ButtonHovered, AccentSurface(ImGuiCol_FrameBg, 0.27f));
        ImGui::PushStyleColor(ImGuiCol_ButtonActive, AccentSurface(ImGuiCol_FrameBg, 0.36f));
    }
    if (EditorSingleLineButton(action.label.c_str())) execute(action.id);
    const std::string tooltip = action.label + (action.description.empty() ? "" : "\n" + action.description);
    EditorTooltip(tooltip.c_str());
    if (action.primary) ImGui::PopStyleColor(3);
    ImGui::EndDisabled();
    ImGui::PopID();
}
} // namespace
void BeginInspectorStyle() {
    ImGui::PushFont(EditorFont(EditorFontRole::Ui), 0);
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
bool InspectorSection(const char* label, bool forceOpen) { return EditorSectionHeader(label, true, forceOpen); }
bool BeginInspectorProperties(const char* id, float labelWidth) {
    constexpr auto flags =
        ImGuiTableFlags_SizingFixedFit | ImGuiTableFlags_NoSavedSettings | ImGuiTableFlags_NoPadOuterX;
    const float width = ImGui::GetContentRegionAvail().x;
    const float scale = InspectorScale();
    labelWidth = std::min(labelWidth, MaximumLabelWidth * scale);
    const float resetWidth = ResetButtonSize * InspectorScale();
    const bool inlineRow =
        width >= labelWidth + resetWidth + MinimumControlWidth * scale + ImGui::GetStyle().CellPadding.x * 6;
    const std::string tableId = std::string("##properties.") + id;
    if (!ImGui::BeginTable(tableId.c_str(), inlineRow ? 3 : 1, flags)) return false;
    if (inlineRow) {
        ImGui::TableSetupColumn("Label", ImGuiTableColumnFlags_WidthFixed, std::max(1.0f, labelWidth));
        ImGui::TableSetupColumn("Value", ImGuiTableColumnFlags_WidthStretch);
        ImGui::TableSetupColumn("Default", ImGuiTableColumnFlags_WidthFixed, resetWidth);
    } else {
        ImGui::TableSetupColumn("Property", ImGuiTableColumnFlags_WidthStretch);
    }
    return true;
}
void EndInspectorProperties() { ImGui::EndTable(); }
bool InspectorParameterField(const PreviewParameter& parameter, PreviewValue& value, bool pending) {
    const bool modified = value != parameter.initial;
    const std::string tooltip = parameter.label + (modified ? "\nModified from default." : "") +
                                (parameter.description.empty() ? "" : "\n" + parameter.description);
    const bool compact = parameter.kind == PreviewParameterKind::Boolean ||
                         (parameter.kind == PreviewParameterKind::Choice && !parameter.choiceButtons);
    if (compact) {
        const float padding = parameter.kind == PreviewParameterKind::Boolean ? CheckboxPaddingY : DropdownPaddingY;
        ImGui::PushStyleVar(ImGuiStyleVar_FramePadding, {ImGui::GetStyle().FramePadding.x, padding * InspectorScale()});
    }
    // Choice grids are standalone full-width blocks; ordinary fields share their group's table.
    const bool choiceGrid = parameter.kind == PreviewParameterKind::Choice && parameter.choiceButtons;
    const bool inlineRow = !choiceGrid && ImGui::TableGetColumnCount() > 1;
    if (choiceGrid) {
        ImGui::BeginGroup();
    } else {
        ImGui::TableNextRow();
        ImGui::TableSetColumnIndex(0);
    }
    if (inlineRow) {
        ImGui::SetCursorPosY(ImGui::GetCursorPosY() + ImGui::GetStyle().FramePadding.y);
        InspectorLabel(parameter.label.c_str(), tooltip.c_str(), modified, ImGui::GetContentRegionAvail().x);
        ImGui::TableSetColumnIndex(2);
    } else {
        const float resetSize = ResetButtonSize * InspectorScale();
        const float gap = ImGui::GetStyle().ItemSpacing.x;
        const float labelWidth = std::max(1.0f, ImGui::GetContentRegionAvail().x - resetSize - gap);
        InspectorLabel(parameter.label.c_str(), tooltip.c_str(), modified, labelWidth);
        ImGui::SameLine(0, gap);
    }
    bool edited = InspectorResetButton(modified, pending, inlineRow);
    if (edited) value = parameter.initial;
    if (inlineRow) ImGui::TableSetColumnIndex(1);
    ImGui::SetNextItemWidth(-FLT_MIN);
    const bool technical = parameter.kind == PreviewParameterKind::Integer ||
                           parameter.kind == PreviewParameterKind::Float ||
                           parameter.kind == PreviewParameterKind::Color;
    if (technical) ImGui::PushFont(EditorFont(EditorFontRole::Technical), 0);
    if (modified) {
        ImGui::PushStyleVar(ImGuiStyleVar_FrameBorderSize, InspectorScale());
        ImGui::PushStyleColor(ImGuiCol_Border, AccentSurface(ImGuiCol_Border, 0.35f));
    }
    edited = EditorParameterWidget(parameter, value) || edited;
    EditorTooltip(parameter.description.c_str());
    if (modified) {
        ImGui::PopStyleColor();
        ImGui::PopStyleVar();
    }
    if (technical) ImGui::PopFont();
    if (pending) {
        EditorStatusBadge("Pending", EditorPhaseColor(EditorPhase::Paused));
        EditorTooltip("Waiting for the preview to accept this value.");
    }
    InspectorPropertyMetadata(parameter.apply);
    if (choiceGrid) ImGui::EndGroup();
    if (compact) ImGui::PopStyleVar();
    return edited;
}
void InspectorReadOnlyField(const PreviewReadout& value) {
    ImGui::TableNextRow();
    ImGui::TableSetColumnIndex(0);
    InspectorLabel(value.label.c_str(), "", false, ImGui::GetContentRegionAvail().x);
    if (ImGui::TableGetColumnCount() > 1) ImGui::TableSetColumnIndex(1);
    ImGui::PushFont(EditorFont(EditorFontRole::Technical), 0);
    InspectorText(value.value.c_str(), ImGui::GetContentRegionAvail().x, true);
    ImGui::PopFont();
}
void InspectorSummary(const char* title, const char* text) {
    if (*title) InspectorText(title, ImGui::GetContentRegionAvail().x);
    if (!*text) return;
    const float width = std::max(1.0f, ImGui::GetContentRegionAvail().x);
    const float lineHeight = ImGui::GetTextLineHeight();
    const auto size = ImGui::CalcTextSize(text, nullptr, false, width);
    const float height = std::min(size.y, lineHeight * DescriptionLines);
    const auto position = ImGui::GetCursorScreenPos();
    ImGui::Dummy({width, height});
    const ImVec4 clip{position.x, position.y, position.x + width, position.y + height};
    ImGui::GetWindowDrawList()->AddText(ImGui::GetFont(), ImGui::GetFontSize(), position,
                                        ImGui::GetColorU32(ImGuiCol_TextDisabled), text, nullptr, width, &clip);
    if (size.y > height) {
        const float suffixWidth = ImGui::CalcTextSize("...").x;
        const ImVec2 suffix{position.x + std::max(0.0f, width - suffixWidth), position.y + height - lineHeight};
        auto* draw = ImGui::GetWindowDrawList();
        draw->AddRectFilled(suffix, {position.x + width, position.y + height}, ImGui::GetColorU32(ImGuiCol_WindowBg));
        draw->AddText(suffix, ImGui::GetColorU32(ImGuiCol_TextDisabled), "...");
    }
    EditorTooltip(text);
}
void InspectorActionGroup(std::span<const PreviewAction> actions, std::span<const std::string> disabled,
                          const std::function<void(const std::string&)>& execute) {
    const auto primary = std::ranges::find(actions, true, &PreviewAction::primary);
    if (primary != actions.end()) InspectorAction(*primary, disabled, execute);
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

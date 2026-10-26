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
constexpr float BaseFontSize = 13.0f;
constexpr float MetadataFontSize = 11.0f;
constexpr float PropertyRowHeight = 33.0f;
constexpr float SectionHeight = 26.0f;
constexpr float SectionSpacing = 6.0f;
constexpr float LabelFraction = 0.39f;

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
void InspectorPropertyRow(const char* label, const char* description, bool metadata) {
    const float scale = InspectorScale();
    ImGui::TableNextRow(ImGuiTableRowFlags_None, PropertyRowHeight * scale);
    ImGui::TableSetColumnIndex(0);
    const auto position = ImGui::GetCursorScreenPos();
    const float height = (PropertyRowHeight + (metadata ? MetadataFontSize + 4.0f : 0.0f)) * scale;
    const float right = ImGui::GetWindowPos().x + ImGui::GetWindowContentRegionMax().x;
    if (ImGui::IsMouseHoveringRect(position, {right, position.y + height})) {
        auto hover = ImGui::GetStyleColorVec4(ImGuiCol_Text);
        hover.w = 0.035f;
        ImGui::TableSetBgColor(ImGuiTableBgTarget_RowBg0, ImGui::GetColorU32(hover));
    }
    ImGui::SetCursorPosY(ImGui::GetCursorPosY() + ImGui::GetStyle().FramePadding.y);
    InspectorText(label, ImGui::GetContentRegionAvail().x);
    if (*description) EditorTooltip(description);
    ImGui::TableSetColumnIndex(1);
    ImGui::SetNextItemWidth(-FLT_MIN);
}
const char* ApplyLabel(PreviewApplyMode apply) {
    switch (apply) {
    case PreviewApplyMode::Immediate : return "Immediate";
    case PreviewApplyMode::NextAction: return "Next action";
    case PreviewApplyMode::OnReset   : return "Reset to apply";
    }
    return "";
}
bool InspectorPropertyMetadata(bool changed, bool pending, PreviewApplyMode apply) {
    ImGui::PushFont(EditorInspectorLabelFont(), ImGui::GetStyle().FontSizeBase * MetadataFontSize / BaseFontSize);
    const float padding = std::clamp((ImGui::GetContentRegionAvail().x - ImGui::CalcTextSize("Default").x) * 0.5f, 0.0f,
                                     3.0f * InspectorScale());
    ImGui::PushStyleVar(ImGuiStyleVar_FramePadding, {padding, 0});
    ImGui::PushStyleColor(ImGuiCol_Button, {0, 0, 0, 0});
    ImGui::PushStyleColor(ImGuiCol_Text, ImGui::GetStyleColorVec4(ImGuiCol_TextDisabled));
    ImGui::BeginDisabled(!changed || pending);
    const bool reset = ImGui::SmallButton("Default");
    ImGui::EndDisabled();
    EditorTooltip("Restore this parameter's default value.");
    ContinueEditorToolbar(ImGui::CalcTextSize(ApplyLabel(apply)).x);
    ImGui::PushTextWrapPos(0);
    ImGui::TextUnformatted(ApplyLabel(apply));
    ImGui::PopTextWrapPos();
    ImGui::PopStyleColor(2);
    ImGui::PopStyleVar();
    ImGui::PopFont();
    return reset;
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
    if (EditorWrappedButton(action.label.c_str())) execute(action.id);
    EditorTooltip(action.description.c_str());
    if (action.primary) ImGui::PopStyleColor(3);
    ImGui::EndDisabled();
    ImGui::PopID();
}
} // namespace
void BeginInspectorStyle() {
    ImGui::PushFont(EditorInspectorLabelFont(), 0);
    const float scale = InspectorScale();
    ImGui::PushStyleVar(ImGuiStyleVar_FramePadding, {7 * scale, 8 * scale});
    ImGui::PushStyleVar(ImGuiStyleVar_ItemSpacing, {6 * scale, 4 * scale});
    ImGui::PushStyleVar(ImGuiStyleVar_CellPadding, {4 * scale, 2 * scale});
    ImGui::PushStyleVar(ImGuiStyleVar_FrameRounding, 4 * scale);
    ImGui::PushStyleVar(ImGuiStyleVar_GrabRounding, 3 * scale);
    ImGui::PushStyleVar(ImGuiStyleVar_FrameBorderSize, scale);
    ImGui::PushStyleColor(ImGuiCol_Button, ImGui::GetStyleColorVec4(ImGuiCol_FrameBg));
    ImGui::PushStyleColor(ImGuiCol_ButtonHovered, ImGui::GetStyleColorVec4(ImGuiCol_FrameBgHovered));
    ImGui::PushStyleColor(ImGuiCol_ButtonActive, ImGui::GetStyleColorVec4(ImGuiCol_FrameBgActive));
}
void EndInspectorStyle() {
    ImGui::PopStyleColor(3);
    ImGui::PopStyleVar(6);
    ImGui::PopFont();
}
bool InspectorSection(const char* label) {
    const float scale = InspectorScale();
    ImGui::Dummy({0, SectionSpacing * scale});
    ImGui::PushStyleVar(ImGuiStyleVar_FramePadding, {4 * scale, (SectionHeight - BaseFontSize) * 0.5f * scale});
    ImGui::PushStyleColor(ImGuiCol_Header, {0, 0, 0, 0});
    ImGui::PushStyleColor(ImGuiCol_HeaderHovered, AccentSurface(ImGuiCol_WindowBg, 0.06f));
    ImGui::PushStyleColor(ImGuiCol_HeaderActive, AccentSurface(ImGuiCol_WindowBg, 0.10f));
    const float labelWidth = ImGui::GetContentRegionAvail().x - ImGui::GetTreeNodeToLabelSpacing() - 8 * scale;
    const auto display = InspectorDisplayText(label, labelWidth);
    // The explicit string ID preserves the existing CollapsingHeader state even when the title truncates.
    const bool expanded = ImGui::TreeNodeEx(label, ImGuiTreeNodeFlags_CollapsingHeader | ImGuiTreeNodeFlags_DefaultOpen,
                                            "%s", display.c_str());
    EditorTooltip(label);
    const auto start = ImGui::GetItemRectMin();
    const auto end = ImGui::GetItemRectMax();
    const float lineStart =
        start.x + ImGui::GetTreeNodeToLabelSpacing() + ImGui::CalcTextSize(display.c_str()).x + 8 * scale;
    if (lineStart < end.x - 4 * scale)
        ImGui::GetWindowDrawList()->AddLine({lineStart, (start.y + end.y) * 0.5f},
                                            {end.x - 4 * scale, (start.y + end.y) * 0.5f},
                                            ImGui::GetColorU32(ImGuiCol_Separator));
    ImGui::PopStyleColor(3);
    ImGui::PopStyleVar();
    return expanded;
}
bool BeginInspectorProperties(const char* id) {
    constexpr auto flags =
        ImGuiTableFlags_SizingFixedFit | ImGuiTableFlags_NoSavedSettings | ImGuiTableFlags_NoPadOuterX;
    const float width = ImGui::GetContentRegionAvail().x;
    // At high DPI / very narrow widths, give controls room and let labels truncate with tooltips.
    const float minimumControl = ImGui::GetFrameHeight() * 2 + ImGui::GetStyle().CellPadding.x * 2;
    const float labelWidth = std::max(1.0f, std::min(width * LabelFraction, width - minimumControl));
    const std::string tableId = std::string("##properties.") + id;
    if (!ImGui::BeginTable(tableId.c_str(), 2, flags)) return false;
    ImGui::TableSetupColumn("Label", ImGuiTableColumnFlags_WidthFixed, labelWidth);
    ImGui::TableSetupColumn("Value", ImGuiTableColumnFlags_WidthStretch);
    return true;
}
void EndInspectorProperties() { ImGui::EndTable(); }
bool InspectorParameterField(const PreviewParameter& parameter, PreviewValue& value, bool pending) {
    const bool modified = value != parameter.initial;
    const std::string label = parameter.label + (modified ? " *" : "") + (pending ? " (pending)" : "");
    const std::string tooltip = label + (parameter.description.empty() ? "" : "\n" + parameter.description);
    InspectorPropertyRow(label.c_str(), tooltip.c_str(), true);
    const bool technical = parameter.kind == PreviewParameterKind::Integer ||
                           parameter.kind == PreviewParameterKind::Float ||
                           parameter.kind == PreviewParameterKind::Color;
    if (technical) ImGui::PushFont(EditorInspectorValueFont(), 0);
    if (modified) ImGui::PushStyleColor(ImGuiCol_Border, AccentSurface(ImGuiCol_Border, 0.35f));
    bool edited = EditorParameterWidget(parameter, value);
    EditorTooltip(parameter.description.c_str());
    if (modified) ImGui::PopStyleColor();
    if (technical) ImGui::PopFont();
    if (InspectorPropertyMetadata(modified, pending, parameter.apply)) {
        value = parameter.initial;
        edited = true;
    }
    return edited;
}
void InspectorReadOnlyField(const PreviewReadout& value) {
    InspectorPropertyRow(value.label.c_str(), "", false);
    ImGui::SetCursorPosY(ImGui::GetCursorPosY() + ImGui::GetStyle().FramePadding.y);
    ImGui::PushFont(EditorInspectorValueFont(), 0);
    InspectorText(value.value.c_str(), ImGui::GetContentRegionAvail().x, true);
    ImGui::PopFont();
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

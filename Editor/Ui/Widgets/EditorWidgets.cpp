#include "Editor/Ui/Widgets/EditorWidgets.h"
#include <imgui.h>
#include <algorithm>
#include <array>
#include <string>
#include <string_view>
#include "Editor/Ui/Style/EditorTheme.h"

namespace {
void DrawWrappedLabel(const char* label, ImVec2 position, ImVec2 size) {
    const ImVec4 clip{position.x, position.y, position.x + size.x, position.y + size.y};
    ImGui::GetWindowDrawList()->AddText(ImGui::GetFont(), ImGui::GetFontSize(), position,
                                        ImGui::GetColorU32(ImGuiCol_Text), label, nullptr, size.x, &clip);
}
std::string ClippedLabel(const char* label, float width) {
    std::string display = label;
    if (ImGui::CalcTextSize(label).x <= width) return display;
    const float suffix = ImGui::CalcTextSize("...").x;
    if (width < suffix) return {};
    while (!display.empty() && ImGui::CalcTextSize(display.c_str()).x + suffix > width) {
        auto end = display.size() - 1;
        while (end > 0 && (static_cast<unsigned char>(display[end]) & 0xc0) == 0x80)
            --end;
        display.resize(end);
    }
    return display + "...";
}
} // namespace
void EditorTooltip(const char* text) {
    if (text && *text && ImGui::IsItemHovered(ImGuiHoveredFlags_AllowWhenDisabled)) {
        ImGui::BeginTooltip();
        ImGui::PushTextWrapPos(ImGui::GetFontSize() * 32);
        ImGui::TextUnformatted(text);
        ImGui::PopTextWrapPos();
        ImGui::EndTooltip();
    }
}
bool EditorSectionHeader(const char* label, bool defaultOpen, bool forceOpen) {
    ImGui::PushFont(EditorFont(EditorFontRole::Ui), 0);
    const float scale = EditorUiScale();
    ImGui::Dummy({0, 4 * scale});
    ImGui::PushStyleVar(ImGuiStyleVar_FramePadding, {4 * scale, 5 * scale});
    ImGui::PushStyleColor(ImGuiCol_Header, {0, 0, 0, 0});
    ImGui::PushStyleColor(ImGuiCol_HeaderHovered, ImGui::GetStyleColorVec4(ImGuiCol_FrameBg));
    ImGui::PushStyleColor(ImGuiCol_HeaderActive, ImGui::GetStyleColorVec4(ImGuiCol_FrameBgHovered));
    const auto display = ClippedLabel(label, ImGui::GetContentRegionAvail().x - ImGui::GetTreeNodeToLabelSpacing());
    ImGuiTreeNodeFlags flags = ImGuiTreeNodeFlags_CollapsingHeader;
    if (defaultOpen) flags |= ImGuiTreeNodeFlags_DefaultOpen;
    if (forceOpen) ImGui::SetNextItemOpen(true, ImGuiCond_Always);
    const bool expanded = ImGui::TreeNodeEx(label, flags, "%s", display.c_str());
    if (display != label) EditorTooltip(label);
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
    ImGui::PopFont();
    return expanded;
}
void EditorEmptyState(const char* title, const char* detail) {
    ImGui::PushFont(EditorFont(EditorFontRole::Ui), 0);
    ImGui::Dummy({0, 8 * EditorUiScale()});
    ImGui::PushTextWrapPos(0);
    ImGui::TextUnformatted(title);
    if (*detail) {
        ImGui::PushStyleColor(ImGuiCol_Text, ImGui::GetStyleColorVec4(ImGuiCol_TextDisabled));
        ImGui::TextUnformatted(detail);
        ImGui::PopStyleColor();
    }
    ImGui::PopTextWrapPos();
    ImGui::PopFont();
}
void EditorStatusBadge(const char* label, ImVec4 color) {
    ImGui::PushFont(EditorFont(EditorFontRole::Ui), 0);
    const float scale = EditorUiScale();
    const ImVec2 padding{6 * scale, 3 * scale};
    const float width = std::max(1.0f, ImGui::GetContentRegionAvail().x);
    const auto display = ClippedLabel(label, std::max(1.0f, width - padding.x * 2));
    const ImVec2 size{std::min(width, ImGui::CalcTextSize(display.c_str()).x + padding.x * 2),
                      ImGui::GetTextLineHeight() + padding.y * 2};
    const auto position = ImGui::GetCursorScreenPos();
    ImGui::Dummy(size);
    auto background = color;
    background.w = 0.12f;
    auto* draw = ImGui::GetWindowDrawList();
    draw->AddRectFilled(position, {position.x + size.x, position.y + size.y}, ImGui::GetColorU32(background),
                        4 * scale);
    draw->AddText({position.x + padding.x, position.y + padding.y}, ImGui::GetColorU32(color), display.c_str());
    if (display != label) EditorTooltip(label);
    ImGui::PopFont();
}
bool EditorSegment(const char* label, bool selected) {
    ImGui::PushFont(EditorFont(EditorFontRole::Ui), 0);
    ImGui::PushStyleColor(ImGuiCol_Button, ImGui::GetStyleColorVec4(selected ? ImGuiCol_Header : ImGuiCol_FrameBg));
    ImGui::PushStyleColor(ImGuiCol_Text, ImGui::GetStyleColorVec4(selected ? ImGuiCol_Text : ImGuiCol_TextDisabled));
    const auto padding = ImGui::GetStyle().FramePadding;
    const std::string_view fullLabel(label);
    const std::string visibleLabel(fullLabel.substr(0, fullLabel.find("##")));
    const float width = std::max(
        1.0f, std::min(ImGui::GetContentRegionAvail().x, ImGui::CalcTextSize(visibleLabel.c_str()).x + padding.x * 2));
    const auto display = ClippedLabel(visibleLabel.c_str(), std::max(1.0f, width - padding.x * 2));
    ImGui::PushID(label);
    const bool pressed = ImGui::Button("##segment", {width, ImGui::GetFrameHeight()});
    const auto position = ImGui::GetItemRectMin();
    DrawWrappedLabel(
        display.c_str(),
        {position.x + std::max(0.0f, (width - ImGui::CalcTextSize(display.c_str()).x) * 0.5f), position.y + padding.y},
        {width, ImGui::GetTextLineHeight()});
    if (display != visibleLabel) EditorTooltip(visibleLabel.c_str());
    ImGui::PopID();
    ImGui::PopStyleColor(2);
    ImGui::PopFont();
    return pressed;
}
bool EditorIconButton(const char* id, EditorGlyph glyph, const char* tooltip, bool selected) {
    ImGui::PushID(id);
    ImGui::PushFont(EditorFont(EditorFontRole::Ui), 0);
    ImGui::PushStyleColor(ImGuiCol_Button, selected ? ImGui::GetStyleColorVec4(ImGuiCol_Header) : ImVec4{0, 0, 0, 0});
    const float size = ImGui::GetFrameHeight();
    const bool pressed = ImGui::Button(EditorGlyphText(glyph), {size, size});
    EditorTooltip(tooltip);
    ImGui::PopStyleColor();
    ImGui::PopFont();
    ImGui::PopID();
    return pressed;
}
void ContinueEditorToolbar(const char* label, bool separateGroup) {
    ContinueEditorToolbar(ImGui::CalcTextSize(label).x + ImGui::GetStyle().FramePadding.x * 2, separateGroup);
}
void ContinueEditorToolbar(float itemWidth, bool separateGroup) {
    const auto& style = ImGui::GetStyle();
    const float spacing = style.ItemSpacing.x * (separateGroup ? 3.0f : 1.0f);
    const float right = ImGui::GetItemRectMax().x + spacing + itemWidth;
    if (right <= ImGui::GetWindowPos().x + ImGui::GetWindowContentRegionMax().x) ImGui::SameLine(0, spacing);
}
bool EditorSingleLineButton(const char* label) {
    const auto padding = ImGui::GetStyle().FramePadding;
    const float width = std::max(1.0f, ImGui::GetContentRegionAvail().x);
    const std::string_view fullLabel(label);
    const std::string visibleLabel(fullLabel.substr(0, fullLabel.find("##")));
    const auto display = ClippedLabel(visibleLabel.c_str(), std::max(1.0f, width - padding.x * 2));
    ImGui::PushID(label);
    const bool pressed = ImGui::Button("##single-line", {width, ImGui::GetFrameHeight()});
    const auto position = ImGui::GetItemRectMin();
    const ImVec4 clip{position.x, position.y, position.x + width, position.y + ImGui::GetFrameHeight()};
    ImGui::GetWindowDrawList()->AddText(
        ImGui::GetFont(), ImGui::GetFontSize(),
        {position.x + std::max(0.0f, (width - ImGui::CalcTextSize(display.c_str()).x) * 0.5f), position.y + padding.y},
        ImGui::GetColorU32(ImGuiCol_Text), display.c_str(), nullptr, 0, &clip);
    if (display != visibleLabel) EditorTooltip(visibleLabel.c_str());
    ImGui::PopID();
    return pressed;
}
bool EditorWrappedSelectable(const char* label, bool selected, ImGuiSelectableFlags flags) {
    const float width = std::max(1.0f, ImGui::GetContentRegionAvail().x);
    const float inset = 6 * EditorUiScale();
    const float textWidth = std::max(1.0f, width - inset);
    const auto textSize = ImGui::CalcTextSize(label, nullptr, false, textWidth);
    const auto position = ImGui::GetCursorScreenPos();
    const float height = std::max(ImGui::GetFrameHeight(), textSize.y + 4 * EditorUiScale());
    ImGui::PushID(label);
    const bool pressed = ImGui::Selectable("##wrapped", selected, flags, {width, height});
    if (selected) {
        const float thickness = 2 * EditorUiScale();
        ImGui::GetWindowDrawList()->AddRectFilled(position, {position.x + thickness, position.y + height},
                                                  ImGui::GetColorU32(ImGuiCol_CheckMark));
    }
    DrawWrappedLabel(label, {position.x + inset, position.y + (height - textSize.y) * 0.5f}, {textWidth, textSize.y});
    ImGui::PopID();
    return pressed;
}
bool EditorParameterWidget(const PreviewParameter& parameter, PreviewValue& value) {
    switch (parameter.kind) {
    case PreviewParameterKind::Integer: {
        auto number = std::get<std::int64_t>(value);
        const auto minimum = static_cast<std::int64_t>(parameter.minimum);
        const auto maximum = static_cast<std::int64_t>(parameter.maximum);
        if (parameter.numericInput || parameter.maximum - parameter.minimum > 10000.0) {
            if (ImGui::InputScalar("##value", ImGuiDataType_S64, &number)) {
                value = std::clamp(number, minimum, maximum);
                return true;
            }
            break;
        }
        if (ImGui::SliderScalar("##value", ImGuiDataType_S64, &number, &minimum, &maximum, nullptr,
                                ImGuiSliderFlags_AlwaysClamp)) {
            value = number;
            return true;
        }
        break;
    }
    case PreviewParameterKind::Float: {
        double number = std::get<double>(value);
        if (parameter.numericInput) {
            if (ImGui::InputDouble("##value", &number, 0.0, 0.0, "%.3f")) {
                value = std::clamp(number, parameter.minimum, parameter.maximum);
                return true;
            }
            break;
        }
        if (ImGui::SliderScalar("##value", ImGuiDataType_Double, &number, &parameter.minimum, &parameter.maximum,
                                "%.3f", ImGuiSliderFlags_AlwaysClamp)) {
            value = number;
            return true;
        }
        break;
    }
    case PreviewParameterKind::Boolean: {
        bool enabled = std::get<bool>(value);
        if (ImGui::Checkbox("##value", &enabled)) {
            value = enabled;
            return true;
        }
        break;
    }
    case PreviewParameterKind::Color: {
        const auto color = std::get<PreviewColor>(value);
        std::array<float, 4> channels{color.r, color.g, color.b, color.a};
        // Keep all RGBA controls in the picker, using a compact hex field in the row.
        const float width = ImGui::CalcItemWidth();
        const auto flags = width < ImGui::GetFontSize() * 9    ? ImGuiColorEditFlags_NoInputs
                           : width < ImGui::GetFontSize() * 22 ? ImGuiColorEditFlags_DisplayHex
                                                               : ImGuiColorEditFlags_None;
        if (ImGui::ColorEdit4("##value", channels.data(), flags)) {
            value = PreviewColor{channels[0], channels[1], channels[2], channels[3]};
            return true;
        }
        break;
    }
    case PreviewParameterKind::Choice: {
        bool changed = false;
        if (parameter.choiceButtons) {
            const auto& style = ImGui::GetStyle();
            float minimumWidth = 0.0f;
            for (const auto& choice : parameter.choices)
                minimumWidth = std::max(minimumWidth, ImGui::CalcTextSize(choice.c_str()).x);
            minimumWidth += style.FramePadding.x * 2 + style.CellPadding.x * 2;
            int columns = parameter.choiceButtonColumns > 0 ? parameter.choiceButtonColumns : 2;
            while (columns > 1 && ImGui::GetContentRegionAvail().x < minimumWidth * static_cast<float>(columns))
                --columns;
            const float width = parameter.choiceButtonColumns > 0 ? std::min(ImGui::GetContentRegionAvail().x,
                                                                             minimumWidth * static_cast<float>(columns))
                                                                  : 0.0f;
            if (ImGui::BeginTable("##choices", columns,
                                  ImGuiTableFlags_SizingStretchSame | ImGuiTableFlags_NoPadOuterX |
                                      ImGuiTableFlags_NoSavedSettings,
                                  {width, 0})) {
                for (const auto& choice : parameter.choices) {
                    ImGui::TableNextColumn();
                    const bool selected = choice == std::get<std::string>(value);
                    if (selected)
                        ImGui::PushStyleColor(ImGuiCol_Button, ImGui::GetStyleColorVec4(ImGuiCol_ButtonActive));
                    if (EditorSingleLineButton(choice.c_str()) && !selected) {
                        value = choice;
                        changed = true;
                    }
                    if (selected) ImGui::PopStyleColor();
                }
                ImGui::EndTable();
            }
            return changed;
        }
        const auto& selected = std::get<std::string>(value);
        const bool open = ImGui::BeginCombo("##value", selected.c_str());
        if (!open) EditorTooltip(selected.c_str());
        if (open) {
            static ImGuiTextFilter filter;
            if (ImGui::IsWindowAppearing()) {
                filter.Clear();
                ImGui::SetKeyboardFocusHere();
            }
            filter.Draw("Search");
            for (const auto& choice : parameter.choices) {
                if (!filter.PassFilter(choice.c_str())) continue;
                if (EditorWrappedSelectable(choice.c_str(), choice == std::get<std::string>(value),
                                            ImGuiSelectableFlags_None)) {
                    value = choice;
                    changed = true;
                }
            }
            ImGui::EndCombo();
        }
        return changed;
    }
    }
    return false;
}

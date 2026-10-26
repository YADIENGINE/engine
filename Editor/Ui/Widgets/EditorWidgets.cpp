#include "Editor/Ui/Widgets/EditorWidgets.h"
#include <imgui.h>
#include <algorithm>
#include <array>

namespace {
void DrawWrappedLabel(const char* label, ImVec2 position, ImVec2 size) {
    const ImVec4 clip{position.x, position.y, position.x + size.x, position.y + size.y};
    ImGui::GetWindowDrawList()->AddText(ImGui::GetFont(), ImGui::GetFontSize(), position,
                                        ImGui::GetColorU32(ImGuiCol_Text), label, nullptr, size.x, &clip);
}
} // namespace
void EditorTooltip(const char* text) {
    if (*text && ImGui::IsItemHovered(ImGuiHoveredFlags_AllowWhenDisabled)) {
        ImGui::BeginTooltip();
        ImGui::PushTextWrapPos(ImGui::GetFontSize() * 32);
        ImGui::TextUnformatted(text);
        ImGui::PopTextWrapPos();
        ImGui::EndTooltip();
    }
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
bool EditorWrappedButton(const char* label) {
    const auto padding = ImGui::GetStyle().FramePadding;
    const float width = std::max(1.0f, ImGui::GetContentRegionAvail().x);
    const float wrapWidth = std::max(1.0f, width - padding.x * 2);
    const auto textSize = ImGui::CalcTextSize(label, nullptr, false, wrapWidth);
    ImGui::PushID(label);
    const bool pressed = ImGui::Button("##wrapped", {width, textSize.y + padding.y * 2});
    const auto position = ImGui::GetItemRectMin();
    DrawWrappedLabel(label, {position.x + std::max(padding.x, (width - textSize.x) * 0.5f), position.y + padding.y},
                     {wrapWidth, textSize.y});
    ImGui::PopID();
    return pressed;
}
bool EditorWrappedSelectable(const char* label, bool selected, ImGuiSelectableFlags flags) {
    const float width = std::max(1.0f, ImGui::GetContentRegionAvail().x);
    const auto textSize = ImGui::CalcTextSize(label, nullptr, false, width);
    const auto position = ImGui::GetCursorScreenPos();
    ImGui::PushID(label);
    const bool pressed = ImGui::Selectable("##wrapped", selected, flags, {width, textSize.y});
    DrawWrappedLabel(label, position, {width, textSize.y});
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
            int columns = parameter.choiceButtonColumns;
            if (columns == 0) {
                const auto& style = ImGui::GetStyle();
                float minimumWidth = 0.0f;
                for (const auto& choice : parameter.choices)
                    minimumWidth = std::max(minimumWidth, ImGui::CalcTextSize(choice.c_str()).x);
                minimumWidth += style.FramePadding.x * 2 + style.CellPadding.x * 2;
                columns = ImGui::GetContentRegionAvail().x >= minimumWidth * 2 ? 2 : 1;
            }
            if (ImGui::BeginTable("##choices", columns,
                                  ImGuiTableFlags_SizingStretchSame | ImGuiTableFlags_NoPadOuterX |
                                      ImGuiTableFlags_NoSavedSettings)) {
                for (const auto& choice : parameter.choices) {
                    ImGui::TableNextColumn();
                    const bool selected = choice == std::get<std::string>(value);
                    if (selected)
                        ImGui::PushStyleColor(ImGuiCol_Button, ImGui::GetStyleColorVec4(ImGuiCol_ButtonActive));
                    if (EditorWrappedButton(choice.c_str()) && !selected) {
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

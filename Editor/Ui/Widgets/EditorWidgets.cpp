#include "Editor/Ui/Widgets/EditorWidgets.h"
#include <imgui.h>
#include <algorithm>
#include <array>
#include <string>
#include "Editor/Ui/Style/EditorTheme.h"
#include "ToolUi/ToolWidgets.h"

bool EditorIconButton(const char* id, EditorGlyph glyph, const char* tooltip, bool selected) {
    return ToolIconButton(id, EditorGlyphText(glyph), tooltip, selected);
}
int EditorChoiceColumns(const SceneParameter& parameter) {
    constexpr std::size_t OneRowChoices = 4;
    if (parameter.choiceButtonColumns > 0) return parameter.choiceButtonColumns;
    return parameter.choices.size() <= OneRowChoices ? std::max(1, static_cast<int>(parameter.choices.size())) : 2;
}
bool EditorParameterWidget(const SceneParameter& parameter, SceneValue& value) {
    switch (parameter.kind) {
    case SceneParameterKind::Integer: {
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
        long long slider = number;
        if (ToolSlider("##value", slider, static_cast<long long>(minimum), static_cast<long long>(maximum))) {
            value = static_cast<std::int64_t>(slider);
            return true;
        }
        break;
    }
    case SceneParameterKind::Float: {
        double number = std::get<double>(value);
        if (parameter.numericInput) {
            if (ImGui::InputDouble("##value", &number, 0.0, 0.0, "%.3f")) {
                value = std::clamp(number, parameter.minimum, parameter.maximum);
                return true;
            }
            break;
        }
        if (ToolSlider("##value", number, parameter.minimum, parameter.maximum)) {
            value = number;
            return true;
        }
        break;
    }
    case SceneParameterKind::Boolean: {
        bool enabled = std::get<bool>(value);
        if (ToolToggle("##value", enabled)) {
            value = enabled;
            return true;
        }
        break;
    }
    case SceneParameterKind::Color: {
        const auto color = std::get<SceneColor>(value);
        std::array<float, 4> channels{color.r, color.g, color.b, color.a};
        // Keep all RGBA controls in the picker, using a compact hex field in the row.
        const float width = ImGui::CalcItemWidth();
        const auto flags = width < ImGui::GetFontSize() * 9    ? ImGuiColorEditFlags_NoInputs
                           : width < ImGui::GetFontSize() * 22 ? ImGuiColorEditFlags_DisplayHex
                                                               : ImGuiColorEditFlags_None;
        if (ImGui::ColorEdit4("##value", channels.data(), flags)) {
            value = SceneColor{channels[0], channels[1], channels[2], channels[3]};
            return true;
        }
        break;
    }
    case SceneParameterKind::Choice: {
        bool changed = false;
        if (parameter.choiceButtons) {
            const auto& style = ImGui::GetStyle();
            float minimumWidth = 0.0f;
            for (const auto& choice : parameter.choices)
                minimumWidth = std::max(minimumWidth, ImGui::CalcTextSize(choice.c_str()).x);
            minimumWidth += style.FramePadding.x * 2 + style.CellPadding.x * 2;
            int columns = EditorChoiceColumns(parameter);
            while (columns > 1 && ImGui::GetContentRegionAvail().x < minimumWidth * static_cast<float>(columns))
                --columns;
            if (ImGui::BeginTable("##choices", columns,
                                  ImGuiTableFlags_SizingStretchSame | ImGuiTableFlags_NoPadOuterX |
                                      ImGuiTableFlags_NoSavedSettings)) {
                for (const auto& choice : parameter.choices) {
                    ImGui::TableNextColumn();
                    const bool selected = choice == std::get<std::string>(value);
                    ImGui::PushStyleColor(ImGuiCol_Button, selected ? ToolPalette::Selected : ToolPalette::Control);
                    ImGui::PushStyleColor(ImGuiCol_ButtonHovered,
                                          selected ? ToolPalette::Selected : ToolPalette::Hover);
                    ImGui::PushStyleColor(ImGuiCol_Text, selected ? ToolPalette::Accent : ToolPalette::Secondary);
                    if (ToolSingleLineButton(choice.c_str()) && !selected) {
                        value = choice;
                        changed = true;
                    }
                    ImGui::PopStyleColor(3);
                }
                ImGui::EndTable();
            }
            return changed;
        }
        const auto& selected = std::get<std::string>(value);
        const bool open = ToolBeginCombo("##value", selected.c_str());
        if (!open) ToolTooltip(selected.c_str());
        if (open) {
            static ImGuiTextFilter filter;
            if (ImGui::IsWindowAppearing()) {
                filter.Clear();
                ImGui::SetKeyboardFocusHere();
            }
            filter.Draw("Search");
            for (const auto& choice : parameter.choices) {
                if (!filter.PassFilter(choice.c_str())) continue;
                if (ToolWrappedSelectable(choice.c_str(), choice == std::get<std::string>(value),
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

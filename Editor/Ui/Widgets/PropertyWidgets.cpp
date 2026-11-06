#include "Editor/Ui/Widgets/PropertyWidgets.h"
#include <imgui.h>
#include <algorithm>
#include <array>
#include <cfloat>
#include <cmath>
#include <limits>
#include <string>
#include "ToolUi/ToolTheme.h"
#include "ToolUi/ToolWidgets.h"

namespace {
constexpr std::size_t OneRowChoices = 4;
constexpr double DragSteps = 400.0; // Pixels to cross a bounded range

// printf format for a number with its unit; '%' in a unit is escaped.
std::string Format(const char* number, const std::string& unit) {
    std::string format = number;
    if (unit.empty()) return format;
    format += ' ';
    for (const char c : unit)
        format += c == '%' ? std::string("%%") : std::string(1, c);
    return format;
}

double Speed(const PropertySpec& spec, double value) {
    if (spec.min && spec.max && *spec.max > *spec.min) return (*spec.max - *spec.min) / DragSteps;
    return std::max(1.0, std::abs(value)) * 0.005;
}

double Lowest(const PropertySpec& spec) { return spec.min.value_or(-DBL_MAX); }
double Highest(const PropertySpec& spec) { return spec.max.value_or(DBL_MAX); }

bool FloatNumber(const PropertySpec& spec, double& number) {
    const auto format = Format("%.6g", spec.unit);
    switch (spec.numbers) {
    case PropertyNumbers::Slider:
        return ToolSlider("##value", number, Lowest(spec), Highest(spec), spec.unit.empty() ? nullptr : format.c_str());
    case PropertyNumbers::Input:
        if (!ImGui::InputDouble("##value", &number, 0.0, 0.0, format.c_str())) return false;
        number = std::clamp(number, Lowest(spec), Highest(spec));
        return true;
    case PropertyNumbers::Drag: {
        const double lowest = Lowest(spec), highest = Highest(spec);
        return ImGui::DragScalar("##value", ImGuiDataType_Double, &number, static_cast<float>(Speed(spec, number)),
                                 &lowest, &highest, format.c_str(), ImGuiSliderFlags_AlwaysClamp);
    }
    }
    return false;
}

bool IntegerNumber(const PropertySpec& spec, std::int64_t& number) {
    const auto lowest =
        static_cast<std::int64_t>(spec.min.value_or(static_cast<double>(std::numeric_limits<int>::min())));
    const auto highest =
        static_cast<std::int64_t>(spec.max.value_or(static_cast<double>(std::numeric_limits<int>::max())));
    const auto format = Format("%lld", spec.unit);
    switch (spec.numbers) {
    case PropertyNumbers::Slider: {
        long long slider = number;
        if (!ToolSlider("##value", slider, lowest, highest, spec.unit.empty() ? nullptr : format.c_str())) return false;
        number = slider;
        return true;
    }
    case PropertyNumbers::Input:
        if (!ImGui::InputScalar("##value", ImGuiDataType_S64, &number, nullptr, nullptr, format.c_str())) return false;
        number = std::clamp(number, lowest, highest);
        return true;
    case PropertyNumbers::Drag:
        return ImGui::DragScalar("##value", ImGuiDataType_S64, &number,
                                 static_cast<float>(std::max(0.05, Speed(spec, static_cast<double>(number)))), &lowest,
                                 &highest, format.c_str(), ImGuiSliderFlags_AlwaysClamp);
    }
    return false;
}

bool Pair(const PropertySpec& spec, float& first, float& second, bool ordered) {
    const auto format = Format("%.6g", spec.unit);
    const auto speed = static_cast<float>(Speed(spec, std::max(std::abs(first), std::abs(second))));
    const auto lowest = static_cast<float>(std::max(Lowest(spec), static_cast<double>(-FLT_MAX)));
    const auto highest = static_cast<float>(std::min(Highest(spec), static_cast<double>(FLT_MAX)));
    if (ordered) // Keeps min <= max while either end is dragged
        return ImGui::DragFloatRange2("##value", &first, &second, speed, lowest, highest, format.c_str(), nullptr,
                                      ImGuiSliderFlags_AlwaysClamp);
    std::array<float, 2> values{first, second};
    if (!ImGui::DragFloat2("##value", values.data(), speed, lowest, highest, format.c_str(),
                           ImGuiSliderFlags_AlwaysClamp))
        return false;
    first = values[0];
    second = values[1];
    return true;
}

bool ColorValue(Color4& color) {
    std::array<float, 4> channels{color.r / 255.0f, color.g / 255.0f, color.b / 255.0f, color.a / 255.0f};
    // Keep all RGBA controls in the picker, using a compact hex field in the row.
    const float width = ImGui::CalcItemWidth();
    const auto flags = width < ImGui::GetFontSize() * 9    ? ImGuiColorEditFlags_NoInputs
                       : width < ImGui::GetFontSize() * 22 ? ImGuiColorEditFlags_DisplayHex
                                                           : ImGuiColorEditFlags_None;
    if (!ImGui::ColorEdit4("##value", channels.data(), flags | ImGuiColorEditFlags_Uint8)) return false;
    const auto byte = [](float channel) {
        return static_cast<std::uint8_t>(std::lround(std::clamp(channel, 0.0f, 1.0f) * 255.0f));
    };
    const Color4 changed{byte(channels[0]), byte(channels[1]), byte(channels[2]), byte(channels[3])};
    if (changed == color) return false;
    color = changed;
    return true;
}

bool ChoiceValue(const PropertySpec& spec, std::size_t& index) {
    const auto name = [&](std::size_t i) { return i < spec.choices.size() ? spec.choices[i].c_str() : "?"; };
    if (spec.choiceButtons) {
        const auto& style = ImGui::GetStyle();
        float minimumWidth = 0.0f;
        for (const auto& choice : spec.choices)
            minimumWidth = std::max(minimumWidth, ImGui::CalcTextSize(choice.c_str()).x);
        minimumWidth += style.FramePadding.x * 2 + style.CellPadding.x * 2;
        int columns = PropertyChoiceColumns(spec);
        while (columns > 1 && ImGui::GetContentRegionAvail().x < minimumWidth * static_cast<float>(columns))
            --columns;
        bool changed = false;
        if (ImGui::BeginTable("##choices", columns,
                              ImGuiTableFlags_SizingStretchSame | ImGuiTableFlags_NoPadOuterX |
                                  ImGuiTableFlags_NoSavedSettings)) {
            for (std::size_t i = 0; i < spec.choices.size(); ++i) {
                ImGui::TableNextColumn();
                ImGui::PushID(static_cast<int>(i)); // Choice names come from the project and may repeat.
                const bool selected = i == index;
                ImGui::PushStyleColor(ImGuiCol_Button, selected ? ToolColors().selected : ToolColors().control);
                ImGui::PushStyleColor(ImGuiCol_ButtonHovered, selected ? ToolColors().selected : ToolColors().hover);
                ImGui::PushStyleColor(ImGuiCol_Text, selected ? ToolColors().accent : ToolColors().secondary);
                if (ToolSingleLineButton(spec.choices[i].c_str()) && !selected) {
                    index = i;
                    changed = true;
                }
                ImGui::PopStyleColor(3);
                ImGui::PopID();
            }
            ImGui::EndTable();
        }
        return changed;
    }
    const bool open = ToolBeginCombo("##value", name(index));
    if (!open) {
        ToolTooltip(name(index));
        return false;
    }
    static ImGuiTextFilter filter;
    if (ImGui::IsWindowAppearing()) {
        filter.Clear();
        ImGui::SetKeyboardFocusHere();
    }
    filter.Draw("Search");
    bool changed = false;
    for (std::size_t i = 0; i < spec.choices.size(); ++i) {
        ImGui::PushID(static_cast<int>(i)); // Choice names come from the project and may repeat.
        if (filter.PassFilter(spec.choices[i].c_str()) &&
            ToolWrappedSelectable(spec.choices[i].c_str(), i == index, ImGuiSelectableFlags_None) && i != index) {
            index = i;
            changed = true;
        }
        ImGui::PopID();
    }
    ImGui::EndCombo();
    return changed;
}
} // namespace

int PropertyChoiceColumns(const PropertySpec& spec) {
    if (spec.choiceButtonColumns > 0) return spec.choiceButtonColumns;
    return spec.choices.size() <= OneRowChoices ? std::max(1, static_cast<int>(spec.choices.size())) : 2;
}

bool PropertyWidget(const PropertySpec& spec, PropertyValue& value) {
    if (auto* number = std::get_if<double>(&value)) return FloatNumber(spec, *number);
    if (auto* integer = std::get_if<std::int64_t>(&value)) return IntegerNumber(spec, *integer);
    if (auto* flag = std::get_if<bool>(&value)) return ToolToggle("##value", *flag);
    if (auto* color = std::get_if<Color4>(&value)) return ColorValue(*color);
    if (auto* point = std::get_if<Float2>(&value)) return Pair(spec, point->x, point->y, false);
    if (auto* range = std::get_if<FloatRange>(&value)) return Pair(spec, range->min, range->max, true);
    return ChoiceValue(spec, std::get<PropertyChoice>(value).index);
}

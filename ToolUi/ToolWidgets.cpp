#include "ToolUi/ToolWidgets.h"
#include <imgui.h>
#include <imgui_internal.h>
#include <algorithm>
#include <cmath>
#include <string>
#include <string_view>
#include "ToolUi/ToolTheme.h"

namespace {
bool PushDisabledTextColor() {
    if ((ImGui::GetCurrentContext()->CurrentItemFlags & ImGuiItemFlags_Disabled) == 0) return false;
    auto color = ToolPalette::Disabled;
    // Preserve the disabled surface fade without applying a second fade to its explicit text color.
    color.w /= std::max(ImGui::GetStyle().Alpha, 0.001f);
    ImGui::PushStyleColor(ImGuiCol_Text, color);
    return true;
}
void PushPrimaryButton() {
    const auto accent = ToolToneColor(ToolTone::Accent);
    ImGui::PushStyleColor(ImGuiCol_Button, accent);
    ImGui::PushStyleColor(ImGuiCol_ButtonHovered, ToolPalette::AccentHover);
    ImGui::PushStyleColor(ImGuiCol_ButtonActive, ToolPalette::AccentPressed);
    ImGui::PushStyleColor(ImGuiCol_Text, ToolPalette::AccentText);
}
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
bool BeginToolPanel(const char* id, ImVec2 size, ToolSurface surface, ImGuiChildFlags flags,
                    ImGuiWindowFlags windowFlags, float padding) {
    ImGui::PushStyleColor(ImGuiCol_ChildBg, ToolSurfaceColor(surface));
    ImGui::PushStyleVar(ImGuiStyleVar_WindowPadding, {padding * ToolUiScale(), padding * ToolUiScale()});
    return ImGui::BeginChild(id, size, flags | ImGuiChildFlags_Borders | ImGuiChildFlags_AlwaysUseWindowPadding,
                             windowFlags);
}
void EndToolPanel() {
    ImGui::EndChild();
    ImGui::PopStyleVar();
    ImGui::PopStyleColor();
}
void ToolTooltip(const char* text) {
    if (text && *text &&
        ImGui::IsItemHovered(ImGuiHoveredFlags_AllowWhenDisabled | ImGuiHoveredFlags_DelayNormal |
                             ImGuiHoveredFlags_Stationary)) {
        ImGui::BeginTooltip();
        ImGui::PushTextWrapPos(ImGui::GetFontSize() * 32);
        ImGui::TextUnformatted(text);
        ImGui::PopTextWrapPos();
        ImGui::EndTooltip();
    }
}
bool ToolSectionHeader(const char* label, bool defaultOpen, bool forceOpen) {
    ImGui::PushFont(ToolFont(ToolFontRole::Heading), ToolMetrics::SectionSize);
    const float scale = ToolUiScale();
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
    if (display != label) ToolTooltip(label);
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
void ToolEmptyState(const char* title, const char* detail) {
    ImGui::PushFont(ToolFont(ToolFontRole::Heading), ToolMetrics::SectionSize);
    ImGui::Dummy({0, 8 * ToolUiScale()});
    ImGui::PushTextWrapPos(0);
    ImGui::TextUnformatted(title);
    ImGui::PopFont();
    ImGui::PushFont(ToolFont(ToolFontRole::Ui), ToolMetrics::BodySize);
    if (*detail) {
        ImGui::PushStyleColor(ImGuiCol_Text, ImGui::GetStyleColorVec4(ImGuiCol_TextDisabled));
        ImGui::TextUnformatted(detail);
        ImGui::PopStyleColor();
    }
    ImGui::PopTextWrapPos();
    ImGui::PopFont();
}
void ToolStatusBadge(const char* label, ImVec4 color) {
    ImGui::PushFont(ToolFont(ToolFontRole::Medium), ToolMetrics::CaptionSize);
    const float scale = ToolUiScale();
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
    if (display != label) ToolTooltip(label);
    ImGui::PopFont();
}
bool ToolButton(const char* label, ImVec2 size, bool primary) {
    ImGui::PushFont(ToolFont(ToolFontRole::Medium), ToolMetrics::BodySize);
    if (primary) PushPrimaryButton();
    const bool disabledText = PushDisabledTextColor();
    const bool pressed = ImGui::Button(label, size);
    if (disabledText) ImGui::PopStyleColor();
    if (primary) ImGui::PopStyleColor(4);
    ImGui::PopFont();
    return pressed;
}
bool ToolSegment(const char* label, bool selected) {
    ImGui::PushFont(ToolFont(ToolFontRole::Medium), ToolMetrics::BodySize);
    ImGui::PushStyleColor(ImGuiCol_Button, ImGui::GetStyleColorVec4(selected ? ImGuiCol_TabSelected : ImGuiCol_Tab));
    ImGui::PushStyleColor(ImGuiCol_Text, ImGui::GetStyleColorVec4(selected ? ImGuiCol_Text : ImGuiCol_TextDisabled));
    auto border = selected ? ToolPalette::Accent : ToolPalette::Border;
    if (selected) border.w = 0.45f;
    ImGui::PushStyleColor(ImGuiCol_Border, border);
    const bool disabledText = PushDisabledTextColor();
    const auto padding = ImGui::GetStyle().FramePadding;
    const std::string_view fullLabel(label);
    const std::string visibleLabel(fullLabel.substr(0, fullLabel.find("##")));
    const float width = std::max(
        1.0f, std::min(ImGui::GetContentRegionAvail().x, ImGui::CalcTextSize(visibleLabel.c_str()).x + padding.x * 2));
    const auto display = ClippedLabel(visibleLabel.c_str(), std::max(1.0f, width - padding.x * 2));
    ImGui::PushID(label);
    const bool pressed = ImGui::Button("##segment", {width, ImGui::GetFrameHeight()});
    const auto position = ImGui::GetItemRectMin();
    if (selected) {
        const auto end = ImGui::GetItemRectMax();
        const float inset = ToolMetrics::Gap * ToolUiScale();
        const float thickness = std::max(1.0f, std::round(2 * ToolUiScale()));
        ImGui::GetWindowDrawList()->AddRectFilled({position.x + inset, end.y - thickness}, {end.x - inset, end.y},
                                                  ImGui::GetColorU32(ToolToneColor(ToolTone::Accent)));
    }
    DrawWrappedLabel(
        display.c_str(),
        {position.x + std::max(0.0f, (width - ImGui::CalcTextSize(display.c_str()).x) * 0.5f), position.y + padding.y},
        {width, ImGui::GetTextLineHeight()});
    if (display != visibleLabel) ToolTooltip(visibleLabel.c_str());
    ImGui::PopID();
    if (disabledText) ImGui::PopStyleColor();
    ImGui::PopStyleColor(3);
    ImGui::PopFont();
    return pressed;
}
bool ToolIconButton(const char* id, const char* icon, const char* tooltip, bool selected) {
    ImGui::PushID(id);
    ImGui::PushFont(ToolFont(ToolFontRole::Ui), 0);
    ImGui::PushStyleColor(ImGuiCol_Button, selected ? ImGui::GetStyleColorVec4(ImGuiCol_Header) : ImVec4{0, 0, 0, 0});
    const bool disabledText = PushDisabledTextColor();
    const float size = ImGui::GetFrameHeight();
    const bool pressed = ImGui::Button(icon, {size, size});
    ToolTooltip(tooltip);
    if (disabledText) ImGui::PopStyleColor();
    ImGui::PopStyleColor();
    ImGui::PopFont();
    ImGui::PopID();
    return pressed;
}
void ContinueToolToolbar(const char* label, bool separateGroup) {
    ContinueToolToolbar(ImGui::CalcTextSize(label).x + ImGui::GetStyle().FramePadding.x * 2, separateGroup);
}
void ContinueToolToolbar(float itemWidth, bool separateGroup) {
    const auto& style = ImGui::GetStyle();
    const float spacing = style.ItemSpacing.x * (separateGroup ? 3.0f : 1.0f);
    const float right = ImGui::GetItemRectMax().x + spacing + itemWidth;
    if (right <= ImGui::GetWindowPos().x + ImGui::GetWindowContentRegionMax().x) ImGui::SameLine(0, spacing);
}
bool ToolSingleLineButton(const char* label, bool primary) {
    ImGui::PushFont(ToolFont(ToolFontRole::Medium), ToolMetrics::BodySize);
    if (primary) PushPrimaryButton();
    const bool disabledText = PushDisabledTextColor();
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
    if (display != visibleLabel) ToolTooltip(visibleLabel.c_str());
    ImGui::PopID();
    if (disabledText) ImGui::PopStyleColor();
    if (primary) ImGui::PopStyleColor(4);
    ImGui::PopFont();
    return pressed;
}
bool ToolWrappedSelectable(const char* label, bool selected, ImGuiSelectableFlags flags) {
    const float width = std::max(1.0f, ImGui::GetContentRegionAvail().x);
    const float inset = 6 * ToolUiScale();
    const float textWidth = std::max(1.0f, width - inset);
    const auto textSize = ImGui::CalcTextSize(label, nullptr, false, textWidth);
    const auto position = ImGui::GetCursorScreenPos();
    const float height = std::max(ImGui::GetFrameHeight(), textSize.y + 4 * ToolUiScale());
    ImGui::PushID(label);
    const bool pressed = ImGui::Selectable("##wrapped", selected, flags, {width, height});
    if (selected) {
        const float thickness = 2 * ToolUiScale();
        ImGui::GetWindowDrawList()->AddRectFilled(position, {position.x + thickness, position.y + height},
                                                  ImGui::GetColorU32(ImGuiCol_CheckMark));
    }
    DrawWrappedLabel(label, {position.x + inset, position.y + (height - textSize.y) * 0.5f}, {textWidth, textSize.y});
    ImGui::PopID();
    return pressed;
}

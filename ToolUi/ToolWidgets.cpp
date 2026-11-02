#include "ToolUi/ToolWidgets.h"
#include <imgui.h>
#include <imgui_internal.h>
#include <algorithm>
#include <cmath>
#include <string>
#include <string_view>
#include <utility>
#include "ToolUi/ToolTheme.h"

namespace {
constexpr float ChevronSize = 8.0f;
constexpr float ChevronStroke = 1.5f;
constexpr float ToggleWidth = 30.0f;
constexpr float ToggleHeight = 16.0f;
constexpr float ToggleInset = 2.0f;
constexpr float SectionPaddingY = 5.0f;
constexpr float SectionTopGap = 4.0f;
constexpr float SectionChevronGap = 6.0f;

bool ItemDisabled() { return (ImGui::GetCurrentContext()->CurrentItemFlags & ImGuiItemFlags_Disabled) != 0; }
bool PushDisabledTextColor() {
    if (!ItemDisabled()) return false;
    auto color = ToolPalette::Disabled;
    // Preserve the disabled surface fade without applying a second fade to its explicit text color.
    color.w /= std::max(ImGui::GetStyle().Alpha, 0.001f);
    ImGui::PushStyleColor(ImGuiCol_Text, color);
    return true;
}
// A disabled main action looks like any disabled control: a faded amber is the least readable state there is.
bool PushPrimaryButton() {
    if (ItemDisabled()) return false;
    ImGui::PushStyleColor(ImGuiCol_Button, ToolPalette::Accent);
    ImGui::PushStyleColor(ImGuiCol_ButtonHovered, ToolPalette::AccentHover);
    ImGui::PushStyleColor(ImGuiCol_ButtonActive, ToolPalette::AccentPressed);
    ImGui::PushStyleColor(ImGuiCol_Text, ToolPalette::AccentText);
    return true;
}
void DrawChevron(ImDrawList& draw, ImVec2 center, float size, ImU32 color, bool down) {
    const float half = size * 0.5f;
    const float quarter = size * 0.25f;
    if (down) {
        draw.PathLineTo({center.x - half, center.y - quarter});
        draw.PathLineTo({center.x, center.y + quarter});
        draw.PathLineTo({center.x + half, center.y - quarter});
    } else {
        draw.PathLineTo({center.x - quarter, center.y - half});
        draw.PathLineTo({center.x + quarter, center.y});
        draw.PathLineTo({center.x - quarter, center.y + half});
    }
    draw.PathStroke(color, ImDrawFlags_None, ChevronStroke * ToolUiScale());
}
void DrawWrappedLabel(const char* label, ImVec2 position, ImVec2 size) {
    const ImVec4 clip{position.x, position.y, position.x + size.x, position.y + size.y};
    ImGui::GetWindowDrawList()->AddText(ImGui::GetFont(), ImGui::GetFontSize(), position,
                                        ImGui::GetColorU32(ImGuiCol_Text), label, nullptr, size.x, &clip);
}
std::size_t PreviousCharacter(std::string_view text, std::size_t end) {
    auto index = end - 1;
    while (index > 0 && (static_cast<unsigned char>(text[index]) & 0xc0) == 0x80)
        --index;
    return index;
}
std::size_t NextCharacter(std::string_view text, std::size_t start) {
    auto index = start + 1;
    while (index < text.size() && (static_cast<unsigned char>(text[index]) & 0xc0) == 0x80)
        ++index;
    return index;
}
} // namespace
std::string ToolClipText(const char* text, float width) {
    std::string display = text;
    if (ImGui::CalcTextSize(text).x <= width) return display;
    const float suffix = ImGui::CalcTextSize("...").x;
    if (width < suffix) return {};
    while (!display.empty() && ImGui::CalcTextSize(display.c_str()).x + suffix > width)
        display.resize(PreviousCharacter(display, display.size()));
    return display + "...";
}
std::string ToolClipMiddle(const char* text, float width) {
    const std::string_view full(text);
    if (ImGui::CalcTextSize(text).x <= width) return std::string(full);
    const float ellipsis = ImGui::CalcTextSize("...").x;
    if (width < ellipsis) return {};
    // Keep more of the end: a path's last folder says the most.
    std::size_t head = 0;
    std::size_t tail = full.size();
    std::string display = "...";
    while (head < tail) {
        const bool growTail = (full.size() - tail) <= head * 2;
        const std::size_t nextHead = growTail ? head : NextCharacter(full, head);
        const std::size_t nextTail = growTail ? PreviousCharacter(full, tail) : tail;
        if (nextHead > nextTail) break;
        std::string candidate = std::string(full.substr(0, nextHead)) + "..." + std::string(full.substr(nextTail));
        if (ImGui::CalcTextSize(candidate.c_str()).x > width) break;
        head = nextHead;
        tail = nextTail;
        display = std::move(candidate);
    }
    return display;
}
bool BeginToolPanel(const char* id, ImVec2 size, ToolSurface surface, ImGuiChildFlags flags,
                    ImGuiWindowFlags windowFlags, float padding) {
    ImGui::PushStyleColor(ImGuiCol_ChildBg, ToolSurfaceColor(surface));
    ImGui::PushStyleVar(ImGuiStyleVar_WindowPadding, {padding * ToolUiScale(), padding * ToolUiScale()});
    return ImGui::BeginChild(id, size, flags | ImGuiChildFlags_AlwaysUseWindowPadding, windowFlags);
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
    const float scale = ToolUiScale();
    ImGui::PushFont(ToolFont(ToolFontRole::Heading), ToolMetrics::SectionSize);
    ImGui::Dummy({0, SectionTopGap * scale});
    const ImGuiID id = ImGui::GetID(label);
    auto* storage = ImGui::GetStateStorage();
    bool open = storage->GetBool(id, defaultOpen);
    const float width = std::max(1.0f, ImGui::GetContentRegionAvail().x);
    const float height = ImGui::GetTextLineHeight() + SectionPaddingY * 2 * scale;
    const float chevron = ChevronSize * scale;
    const float textStart = chevron + SectionChevronGap * scale;
    const auto display = ToolClipText(label, std::max(1.0f, width - textStart));
    const auto start = ImGui::GetCursorScreenPos();
    if (ImGui::InvisibleButton(label, {width, height})) {
        open = !open;
        storage->SetBool(id, open);
    }
    if (forceOpen) open = true;
    const bool hovered = ImGui::IsItemHovered();
    if (display != label) ToolTooltip(label);
    auto* draw = ImGui::GetWindowDrawList();
    if (hovered)
        draw->AddRectFilled(start, {start.x + width, start.y + height}, ImGui::GetColorU32(ImGuiCol_HeaderHovered),
                            ToolMetrics::Radius * scale);
    const auto color = ImGui::GetColorU32(hovered ? ToolPalette::Text : ToolPalette::Secondary);
    DrawChevron(*draw, {start.x + chevron * 0.5f + scale, start.y + height * 0.5f}, chevron, color, open);
    // RenderText, not AddText: an open ImGui log (tests, copy) records it.
    ImGui::PushStyleColor(ImGuiCol_Text, color);
    ImGui::RenderText({start.x + textStart + scale, start.y + SectionPaddingY * scale}, display.c_str());
    ImGui::PopStyleColor();
    ImGui::PopFont();
    return open;
}
void ToolEmptyState(const char* title, const char* detail) {
    ImGui::PushFont(ToolFont(ToolFontRole::Heading), ToolMetrics::BodySize);
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
    const auto display = ToolClipText(label, std::max(1.0f, width - padding.x * 2));
    const ImVec2 size{std::min(width, ImGui::CalcTextSize(display.c_str()).x + padding.x * 2),
                      ImGui::GetTextLineHeight() + padding.y * 2};
    const auto position = ImGui::GetCursorScreenPos();
    ImGui::Dummy(size);
    auto background = color;
    background.w = 0.12f;
    auto* draw = ImGui::GetWindowDrawList();
    draw->AddRectFilled(position, {position.x + size.x, position.y + size.y}, ImGui::GetColorU32(background),
                        ToolMetrics::Radius * scale);
    draw->AddText({position.x + padding.x, position.y + padding.y}, ImGui::GetColorU32(color), display.c_str());
    if (display != label) ToolTooltip(label);
    ImGui::PopFont();
}
bool ToolButton(const char* label, ImVec2 size, bool primary) {
    ImGui::PushFont(ToolFont(ToolFontRole::Medium), ToolMetrics::BodySize);
    const bool accent = primary && PushPrimaryButton();
    const bool disabledText = PushDisabledTextColor();
    const bool pressed = ImGui::Button(label, size);
    if (disabledText) ImGui::PopStyleColor();
    if (accent) ImGui::PopStyleColor(4);
    ImGui::PopFont();
    return pressed;
}
bool ToolSegment(const char* label, bool selected, ImVec4 attention) {
    ImGui::PushFont(ToolFont(ToolFontRole::Medium), ToolMetrics::BodySize);
    ImGui::PushStyleColor(ImGuiCol_Button, attention);
    ImGui::PushStyleColor(ImGuiCol_ButtonHovered, attention.w > 0 ? attention : ToolPalette::Elevated);
    ImGui::PushStyleColor(ImGuiCol_ButtonActive, attention.w > 0 ? attention : ToolPalette::Pressed);
    ImGui::PushStyleColor(ImGuiCol_Text,
                          selected ? ToolPalette::Text : ImGui::GetStyleColorVec4(ImGuiCol_TextDisabled));
    const bool disabledText = PushDisabledTextColor();
    const auto padding = ImGui::GetStyle().FramePadding;
    const std::string_view fullLabel(label);
    const std::string visibleLabel(fullLabel.substr(0, fullLabel.find("##")));
    const float width = std::max(
        1.0f, std::min(ImGui::GetContentRegionAvail().x, ImGui::CalcTextSize(visibleLabel.c_str()).x + padding.x * 2));
    const auto display = ToolClipText(visibleLabel.c_str(), std::max(1.0f, width - padding.x * 2));
    ImGui::PushID(label);
    const bool pressed = ImGui::Button("##segment", {width, ImGui::GetFrameHeight()});
    const auto position = ImGui::GetItemRectMin();
    if (selected) {
        const auto end = ImGui::GetItemRectMax();
        const float inset = padding.x;
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
    ImGui::PopStyleColor(4);
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
float ToolToggleWidth(const char* label) {
    const float scale = ToolUiScale();
    const auto text = ImGui::CalcTextSize(label, nullptr, true);
    return ToggleWidth * scale + (text.x > 0 ? ImGui::GetStyle().ItemInnerSpacing.x + text.x : 0);
}
bool ToolToggle(const char* label, bool& value) {
    const float scale = ToolUiScale();
    const ImVec2 track{std::round(ToggleWidth * scale), std::round(ToggleHeight * scale)};
    const float width = ToolToggleWidth(label);
    const float itemHeight = std::max(track.y, ImGui::GetFrameHeight());
    const auto start = ImGui::GetCursorScreenPos();
    const bool pressed = ImGui::InvisibleButton(label, {width, itemHeight});
    if (pressed) value = !value;
    const bool hovered = ImGui::IsItemHovered();
    auto* draw = ImGui::GetWindowDrawList();
    const float radius = 2 * scale;
    const ImVec2 trackStart{std::floor(start.x), std::floor(start.y + (itemHeight - track.y) * 0.5f)};
    const ImVec4 trackColor = value ? (hovered ? ToolPalette::AccentHover : ToolPalette::Accent)
                                    : (hovered ? ToolPalette::Hover : ToolPalette::Control);
    draw->AddRectFilled(trackStart, {trackStart.x + track.x, trackStart.y + track.y}, ImGui::GetColorU32(trackColor),
                        radius);
    const float inset = std::max(1.0f, std::round(ToggleInset * scale));
    const float knob = track.y - inset * 2;
    const float knobX = value ? trackStart.x + track.x - inset - knob : trackStart.x + inset;
    draw->AddRectFilled({knobX, trackStart.y + inset}, {knobX + knob, trackStart.y + inset + knob},
                        ImGui::GetColorU32(value ? ToolPalette::AccentText : ToolPalette::Muted), radius * 0.5f);
    if (const char* textEnd = ImGui::FindRenderedTextEnd(label); textEnd != label) {
        const float textY = start.y + (itemHeight - ImGui::GetTextLineHeight()) * 0.5f;
        ImGui::RenderText({trackStart.x + track.x + ImGui::GetStyle().ItemInnerSpacing.x, textY}, label, textEnd);
    }
    return pressed;
}
bool ToolBeginCombo(const char* label, const char* preview, ImGuiComboFlags flags) {
    const float scale = ToolUiScale();
    const float width = ImGui::CalcItemWidth();
    const float height = ImGui::GetFrameHeight();
    const float paddingX = ImGui::GetStyle().FramePadding.x;
    const float chevron = ChevronSize * scale;
    const auto start = ImGui::GetCursorScreenPos();
    const auto display = ToolClipText(preview, std::max(1.0f, width - paddingX * 2 - chevron - ToolMetrics::Space));
    const auto color = ImGui::GetColorU32(ToolPalette::Secondary);
    auto* draw = ImGui::GetWindowDrawList();
    const bool open = ImGui::BeginCombo(label, display.c_str(), flags | ImGuiComboFlags_NoArrowButton);
    // The parent's list: BeginCombo may have switched to the popup's window.
    DrawChevron(*draw, {start.x + width - paddingX - chevron * 0.5f, start.y + height * 0.5f}, chevron, color, true);
    return open;
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
    const bool accent = primary && PushPrimaryButton();
    const bool disabledText = PushDisabledTextColor();
    const auto padding = ImGui::GetStyle().FramePadding;
    const float width = std::max(1.0f, ImGui::GetContentRegionAvail().x);
    const std::string_view fullLabel(label);
    const std::string visibleLabel(fullLabel.substr(0, fullLabel.find("##")));
    const auto display = ToolClipText(visibleLabel.c_str(), std::max(1.0f, width - padding.x * 2));
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
    if (accent) ImGui::PopStyleColor(4);
    ImGui::PopFont();
    return pressed;
}
bool ToolWrappedSelectable(const char* label, bool selected, ImGuiSelectableFlags flags, float reservedRight) {
    const float width = std::max(1.0f, ImGui::GetContentRegionAvail().x);
    const float inset = 6 * ToolUiScale();
    const float textWidth = std::max(1.0f, width - inset * 2 - reservedRight);
    const auto textSize = ImGui::CalcTextSize(label, nullptr, false, textWidth);
    const auto position = ImGui::GetCursorScreenPos();
    const float height = std::max(ImGui::GetFrameHeight(), textSize.y + 4 * ToolUiScale());
    ImGui::PushID(label);
    const bool pressed = ImGui::Selectable("##wrapped", selected, flags, {width, height});
    DrawWrappedLabel(label, {position.x + inset, position.y + (height - textSize.y) * 0.5f}, {textWidth, textSize.y});
    ImGui::PopID();
    return pressed;
}

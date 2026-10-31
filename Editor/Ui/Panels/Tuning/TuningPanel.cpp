#include "Editor/Ui/Panels/Tuning/TuningPanel.h"
#include <imgui.h>
#include <algorithm>
#include <array>
#include <charconv>
#include <cstdio>
#include <optional>
#include <vector>
#include "Editor/Internals/Session/TuningWorkspace.h"
#include "Editor/Ui/Widgets/InspectorWidgets.h"
#include "ToolUi/ToolTheme.h"
#include "ToolUi/ToolWidgets.h"

namespace {
constexpr float ValueWidth = 92.0f; // Tek sayı kutusu (ölçeksiz)
constexpr float ColorTextWidth = 110.0f;
constexpr float RejectedAlpha = 0.45f;

enum class Edit { None, Commit, Invalid };

// Bir çizim karesinin düzenleme bağlamı.
struct Editing {
    std::size_t file = 0;
    const EditorUiActions& actions;
    std::string& rejected;
    std::string& rejectedText;
};

std::string Key(const TuningLocation& at) {
    return at.field + "|" + std::to_string(at.row) + "|" + at.column + "|" + std::to_string(at.element);
}

std::string Shown(double number) {
    std::array<char, 32> buffer{};
    const auto result = std::to_chars(buffer.data(), buffer.data() + buffer.size(), number);
    return {buffer.data(), result.ptr};
}

// Gönderilmeden önce sınır denetimi; asıl doğrulamayı yazarken TuningFile yapar.
std::optional<std::string> Problem(const TuningSpec& spec, const TuningValue& value) {
    std::vector<double> numbers;
    if (const auto* number = std::get_if<float>(&value)) numbers = {*number};
    if (const auto* integer = std::get_if<int>(&value)) numbers = {static_cast<double>(*integer)};
    if (const auto* point = std::get_if<Float2>(&value)) numbers = {point->x, point->y};
    if (const auto* range = std::get_if<FloatRange>(&value)) {
        if (range->min > range->max) return "min is greater than max";
        numbers = {range->min, range->max};
    }
    for (const double number : numbers)
        if ((spec.min && number < *spec.min) || (spec.max && number > *spec.max))
            return Shown(number) + " is outside " + (spec.min ? Shown(*spec.min) : "*") + ".." +
                   (spec.max ? Shown(*spec.max) : "*");
    return std::nullopt;
}

std::string ColorText(Color4 color) {
    std::array<char, 10> text{};
    if (color.a == 255)
        std::snprintf(text.data(), text.size(), "#%02X%02X%02X", color.r, color.g, color.b);
    else
        std::snprintf(text.data(), text.size(), "#%02X%02X%02X%02X", color.r, color.g, color.b, color.a);
    return text.data();
}

std::optional<Color4> ParseColor(std::string_view text) {
    if ((text.size() != 7 && text.size() != 9) || text[0] != '#') return std::nullopt;
    std::array<std::uint8_t, 4> channels{0, 0, 0, 255};
    for (std::size_t i = 1; i < text.size(); i += 2) {
        unsigned channel = 0;
        const auto result = std::from_chars(text.data() + i, text.data() + i + 2, channel, 16);
        if (result.ec != std::errc{} || result.ptr != text.data() + i + 2) return std::nullopt;
        channels[i / 2] = static_cast<std::uint8_t>(channel);
    }
    return Color4{channels[0], channels[1], channels[2], channels[3]};
}

// Tek değerin kutusu. Sayılar ve renk Enter'la girer; onay kutusu tıklanınca.
Edit Control(const char* id, TuningValue& value, float width) {
    constexpr auto Enter = ImGuiInputTextFlags_EnterReturnsTrue;
    ImGui::SetNextItemWidth(width);
    if (auto* number = std::get_if<float>(&value))
        return ImGui::InputFloat(id, number, 0, 0, "%g", Enter) ? Edit::Commit : Edit::None;
    if (auto* integer = std::get_if<int>(&value))
        return ImGui::InputInt(id, integer, 0, 0, Enter) ? Edit::Commit : Edit::None;
    if (auto* flag = std::get_if<bool>(&value)) return ImGui::Checkbox(id, flag) ? Edit::Commit : Edit::None;
    if (auto* point = std::get_if<Float2>(&value)) {
        std::array<float, 2> pair{point->x, point->y};
        ImGui::SetNextItemWidth(width * 2);
        if (!ImGui::InputFloat2(id, pair.data(), "%g", Enter)) return Edit::None;
        *point = {pair[0], pair[1]};
        return Edit::Commit;
    }
    if (auto* range = std::get_if<FloatRange>(&value)) {
        std::array<float, 2> pair{range->min, range->max};
        ImGui::SetNextItemWidth(width * 2);
        if (!ImGui::InputFloat2(id, pair.data(), "%g", Enter)) return Edit::None;
        *range = {pair[0], pair[1]};
        return Edit::Commit;
    }
    auto& color = std::get<Color4>(value);
    const ImVec4 swatch{color.r / 255.0f, color.g / 255.0f, color.b / 255.0f, color.a / 255.0f};
    ImGui::ColorButton((std::string(id) + ".swatch").c_str(), swatch, ImGuiColorEditFlags_AlphaPreviewHalf);
    ImGui::SameLine();
    std::array<char, 16> text{};
    const std::string current = ColorText(color);
    std::copy(current.begin(), current.end(), text.begin());
    ImGui::SetNextItemWidth(ColorTextWidth * ToolUiScale());
    if (!ImGui::InputText(id, text.data(), text.size(), Enter | ImGuiInputTextFlags_CharsNoBlank)) return Edit::None;
    const auto parsed = ParseColor(text.data());
    if (!parsed) return Edit::Invalid;
    color = *parsed;
    return Edit::Commit;
}

void Value(Editing& editing, const TuningLocation& at, const TuningSpec& spec, TuningValue value) {
    const std::string key = Key(at);
    const bool rejected = editing.rejected == key;
    if (rejected) {
        auto tone = ToolToneColor(ToolTone::Error);
        tone.w = RejectedAlpha;
        ImGui::PushStyleColor(ImGuiCol_FrameBg, tone);
    }
    const auto edit = Control(("##" + key).c_str(), value, ValueWidth * ToolUiScale());
    if (rejected) {
        ImGui::PopStyleColor();
        if (ImGui::IsItemHovered()) ToolTooltip(editing.rejectedText.c_str());
    }
    if (edit == Edit::None) return;
    const auto problem =
        edit == Edit::Invalid ? std::optional<std::string>("expected #RRGGBB or #RRGGBBAA") : Problem(spec, value);
    if (problem) {
        editing.rejected = key;
        editing.rejectedText = *problem;
        return;
    }
    if (rejected) editing.rejected.clear();
    editing.actions.tune(editing.file, at, value);
}

// Liste: kutular yan yana, sığmayan alt satıra.
void Values(Editing& editing, TuningLocation at, const TuningSpec& spec, const std::vector<TuningValue>& values) {
    const float right = ImGui::GetCursorScreenPos().x + ImGui::GetContentRegionAvail().x;
    for (std::size_t i = 0; i < values.size(); ++i) {
        at.element = i;
        if (i > 0) {
            ImGui::SameLine();
            if (ImGui::GetCursorScreenPos().x + ValueWidth * ToolUiScale() > right) ImGui::NewLine();
        }
        Value(editing, at, spec, values[i]);
    }
}

void Status(const TuningFileState& file, const EditorSnapshot& snapshot, const EditorUiActions& actions) {
    if (file.problem) {
        ImGui::PushStyleColor(ImGuiCol_Text, ToolToneColor(ToolTone::Error));
        ImGui::TextWrapped("Line %d: %s", file.problem->line, file.problem->text.c_str());
        ImGui::PopStyleColor();
        ImGui::TextDisabled("Showing the last valid values; fix the file to edit it here.");
        return;
    }
    if (!file.worker) {
        ImGui::TextDisabled(snapshot.ready ? "The running preview does not use this file."
                                           : "Open a preview to see changes live.");
        return;
    }
    const auto& worker = *file.worker;
    switch (worker.state) {
    case PreviewTuningState::Applied: ToolStatusBadge("Live", ToolToneColor(ToolTone::Success)); break;
    case PreviewTuningState::Invalid:
        ToolStatusBadge("Not applied", ToolToneColor(ToolTone::Error));
        ImGui::TextWrapped("Line %d: %s", worker.line, worker.text.c_str());
        break;
    case PreviewTuningState::NeedsBuild:
        ToolStatusBadge("Build needed", ToolToneColor(ToolTone::Warning));
        ImGui::TextWrapped("%s", worker.text.c_str());
        if (snapshot.capabilities.Can(EditorCommand::Refresh) && ImGui::Button("Build & Refresh"))
            actions.command(EditorCommand::Refresh);
        break;
    case PreviewTuningState::Unreadable:
        ToolStatusBadge("Unreadable", ToolToneColor(ToolTone::Error));
        ImGui::TextWrapped("%s", worker.text.c_str());
        break;
    }
}

const char* Suffix(const TuningFileState& file) {
    if (file.problem) return "  (invalid)";
    if (file.worker && file.worker->state == PreviewTuningState::NeedsBuild) return "  (build needed)";
    return "";
}

void Fields(Editing& editing, const TuningFileState& file) {
    float labelWidth = 0;
    for (const auto& field : file.fields)
        if (!field.table) labelWidth = std::max(labelWidth, ImGui::CalcTextSize(field.name.c_str()).x);
    if (BeginInspectorProperties("tuning", labelWidth, true)) {
        const bool inlineRow = ImGui::TableGetColumnCount() > 1;
        for (const auto& field : file.fields) {
            if (field.table) continue;
            ImGui::TableNextRow();
            ImGui::TableSetColumnIndex(0);
            ImGui::AlignTextToFramePadding();
            ImGui::TextUnformatted(field.name.c_str());
            if (inlineRow) ImGui::TableSetColumnIndex(1);
            Values(editing, {field.name}, field.spec, field.values);
        }
        EndInspectorProperties();
    }
    for (const auto& field : file.fields) {
        if (!field.table || !InspectorSection(field.name.c_str())) continue;
        constexpr auto flags = ImGuiTableFlags_Borders | ImGuiTableFlags_RowBg | ImGuiTableFlags_SizingFixedFit |
                               ImGuiTableFlags_ScrollX | ImGuiTableFlags_NoSavedSettings;
        const int columns = static_cast<int>(field.columns.size()) + 1;
        if (!ImGui::BeginTable(("##table." + field.name).c_str(), columns, flags)) continue;
        ImGui::TableSetupColumn("#");
        for (const auto& column : field.columns)
            ImGui::TableSetupColumn(column.name.c_str());
        ImGui::TableHeadersRow();
        for (std::size_t row = 0; row < field.rows.size(); ++row) {
            ImGui::TableNextRow();
            ImGui::TableSetColumnIndex(0);
            ImGui::AlignTextToFramePadding();
            ImGui::Text("%zu", row);
            for (std::size_t c = 0; c < field.columns.size(); ++c) {
                ImGui::TableSetColumnIndex(static_cast<int>(c) + 1);
                const auto& column = field.columns[c];
                for (std::size_t i = 0; i < field.rows[row][c].size(); ++i) {
                    if (i > 0) ImGui::SameLine();
                    Value(editing, {field.name, row, column.name, i}, column.spec, field.rows[row][c][i]);
                }
            }
        }
        ImGui::EndTable();
    }
}
} // namespace

void TuningPanel::Draw(const EditorSnapshot& snapshot, const EditorUiActions& actions) {
    BeginInspectorStyle();
    const auto* files = snapshot.tuning ? &snapshot.tuning->Files() : nullptr;
    if (!files || files->empty()) {
        ToolEmptyState("No tuning files",
                       "Put X.tuning.json next to a class and give its target engine_add_tuning in CMake; "
                       "the list refreshes after the next configure.");
        EndInspectorStyle();
        return;
    }
    auto selected = std::ranges::find(*files, _file, &TuningFileState::source);
    if (selected == files->end()) {
        selected = files->begin();
        _file = selected->source;
        _rejected.clear();
    }
    ImGui::SetNextItemWidth(-1);
    if (ImGui::BeginCombo("##tuning.file", (selected->label + Suffix(*selected)).c_str())) {
        for (const auto& file : *files) {
            const bool current = file.source == _file;
            if (ImGui::Selectable((file.label + Suffix(file) + "##" + file.label).c_str(), current)) {
                _file = file.source;
                _rejected.clear();
            }
            if (current) ImGui::SetItemDefaultFocus();
        }
        ImGui::EndCombo();
    }
    if (ImGui::IsItemHovered()) {
        const auto path = selected->source.generic_u8string();
        ToolTooltip(std::string(path.begin(), path.end()).c_str());
    }
    Status(*selected, snapshot, actions);
    ImGui::Separator();
    Editing editing{static_cast<std::size_t>(selected - files->begin()), actions, _rejected, _rejectedText};
    ImGui::PushID(selected->label.c_str());
    if (selected->problem) ImGui::BeginDisabled();
    Fields(editing, *selected);
    if (selected->problem) ImGui::EndDisabled();
    ImGui::PopID();
    EndInspectorStyle();
}

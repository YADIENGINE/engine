#include "Editor/Ui/Panels/Inspector/TuningInspector.h"
#include <imgui.h>
#include <algorithm>
#include <cfloat>
#include <vector>
#include "Editor/Internals/Diagnostics/EditorDiagnostics.h"
#include "Editor/Internals/Session/AgentContext.h"
#include "Editor/Internals/Session/QuickOpen.h"
#include "Editor/Ui/Widgets/InspectorWidgets.h"
#include "Editor/Ui/Widgets/PropertyWidgets.h"
#include "ToolUi/ToolTheme.h"
#include "ToolUi/ToolWidgets.h"

namespace {
constexpr float CellWidth = 92.0f; // A list element or table cell (unscaled)
constexpr const char* ContextPopup = "tuning-context";
constexpr const char* RevertTooltip = "Revert: the value in the last commit (git HEAD).";

std::string Utf8(const std::filesystem::path& file) {
    const auto text = file.generic_u8string();
    return {text.begin(), text.end()};
}

std::optional<TuningValue> ValueAt(const TuningField& field, const TuningLocation& at) {
    const std::vector<TuningValue>* values = &field.values;
    if (field.table) {
        const auto column = std::ranges::find(field.columns, at.column, &TuningColumn::name);
        if (column == field.columns.end() || at.row >= field.rows.size()) return std::nullopt;
        values = &field.rows[at.row][static_cast<std::size_t>(column - field.columns.begin())];
    }
    if (at.element >= values->size()) return std::nullopt;
    return (*values)[at.element];
}

// Differs from the committed file; a field the commit lacks is new, so modified. Unknown without a commit.
bool Modified(const TuningFileState& file, const TuningField& field) {
    if (!file.committed) return false;
    const auto* committed = file.CommittedField(field.name);
    if (!committed) return true;
    if (committed->table != field.table || committed->values != field.values || committed->rows != field.rows)
        return true;
    return !std::ranges::equal(committed->columns, field.columns,
                               [](const TuningColumn& a, const TuningColumn& b) { return a.name == b.name; });
}

bool Modified(const TuningFileState& file, const TuningField& field, const TuningLocation& at) {
    if (!file.committed) return false;
    const auto* committed = file.CommittedField(field.name);
    return !committed || ValueAt(*committed, at) != ValueAt(field, at);
}

bool Matches(const TuningFileState& file, const TuningField& field, const std::string& query, bool modifiedOnly) {
    if (modifiedOnly && !Modified(file, field)) return false;
    if (query.empty()) return true;
    return QuickOpenScore(field.name,
                          PropertyLabel(field.name) + " " + field.group + " " + field.note + " " + file.label,
                          query) >= 0;
}

std::string ValuesText(const TuningSpec& spec, const std::vector<TuningValue>& values) {
    const auto property = TuningPropertySpec(spec);
    if (!spec.list) return values.empty() ? std::string() : PropertyText(property, TuningPropertyValue(values[0]));
    std::string text = "[";
    for (std::size_t i = 0; i < values.size(); ++i)
        text += (i ? ", " : "") + PropertyText(property, TuningPropertyValue(values[i]));
    return text + "]";
}

std::string FieldText(const TuningField& field) {
    if (field.table) return "table of " + std::to_string(field.rows.size()) + " rows";
    return ValuesText(field.spec, field.values);
}

// What the hover text and the agent line add after the value.
std::string Details(const TuningFileState& file, const TuningField& field) {
    std::string text;
    if (!field.table) {
        const auto bounds = PropertyBounds(TuningPropertySpec(field.spec));
        if (!bounds.empty()) text += "Bounds: " + bounds + "\n";
        if (!field.spec.choices.empty()) {
            text += "Choices:";
            for (const auto& choice : field.spec.choices)
                text += " " + choice;
            text += "\n";
        }
    }
    if (!file.committed) {
        text += "No committed version to compare with.\n";
    } else if (const auto* committed = file.CommittedField(field.name); !committed) {
        text += "New since the last commit.\n";
    } else if (Modified(file, field)) {
        text += "Committed: " + FieldText(*committed) + "\n";
    }
    if (!field.note.empty()) text += field.note + "\n";
    return text;
}

std::string Tooltip(const TuningFileState& file, const TuningField& field) {
    return PropertyLabel(field.name) + " (" + field.name + ")\nSaved in " + file.relative + ", line " +
           std::to_string(field.line) + "\n" + Details(file, field) +
           "Right-click: revert, open in the code editor, send to the agent. Ctrl+Z undoes.";
}

void BuildButton(const EditorSnapshot& snapshot, const EditorUiActions& actions) {
    if (snapshot.capabilities.Can(EditorCommand::Refresh) && ToolButton("Build & Refresh"))
        actions.command(EditorCommand::Refresh);
}
} // namespace

bool TuningInspector::Shows(const TuningFileState& file, const std::string& query, bool modifiedOnly) {
    return std::ranges::any_of(file.fields,
                               [&](const TuningField& field) { return Matches(file, field, query, modifiedOnly); });
}

std::size_t TuningInspector::ModifiedCount(const TuningFileState& file) {
    return static_cast<std::size_t>(
        std::ranges::count_if(file.fields, [&](const TuningField& field) { return Modified(file, field); }));
}

void TuningInspector::File(const TuningFileState& file, const EditorSnapshot& snapshot, const EditorUiActions& actions,
                           const std::string& query, bool modifiedOnly, bool standalone) {
    ImGui::PushID(Utf8(file.source).c_str());
    _popup = ImGui::GetID(ContextPopup);
    Status(file, snapshot, actions, standalone);
    ImGui::BeginDisabled(file.problem.has_value());
    Fields(file, snapshot, actions, query, modifiedOnly);
    ImGui::EndDisabled();
    ContextMenu(file, snapshot, actions);
    ImGui::PopID();
}

void TuningInspector::Status(const TuningFileState& file, const EditorSnapshot& snapshot,
                             const EditorUiActions& actions, bool standalone) {
    if (file.problem) {
        ImGui::PushStyleColor(ImGuiCol_Text, ToolToneColor(ToolTone::Error));
        ImGui::TextWrapped("Line %d: %s", file.problem->line, file.problem->text.c_str());
        ImGui::PopStyleColor();
        ImGui::TextDisabled("Showing the last valid values; fix the file to edit it here.");
        return;
    }
    if (file.target.empty()) {
        ToolStatusBadge("Not built", ToolToneColor(ToolTone::Warning));
        ToolTooltip("No build has made its header yet, so no code can read it.");
        ImGui::SameLine();
        BuildButton(snapshot, actions);
        return;
    }
    if (file.used && file.worker) {
        const auto& worker = *file.worker;
        switch (worker.state) {
        case WorkerTuningState::Applied:
            ToolStatusBadge("Live", ToolToneColor(ToolTone::Success));
            ToolTooltip("The running scene reads this file; changes show at once.");
            break;
        case WorkerTuningState::Invalid:
            ToolStatusBadge("Not applied", ToolToneColor(ToolTone::Error));
            ImGui::TextWrapped("Line %d: %s", worker.line, worker.text.c_str());
            break;
        case WorkerTuningState::NeedsBuild:
            ToolStatusBadge("Build needed", ToolToneColor(ToolTone::Warning));
            ImGui::SameLine();
            BuildButton(snapshot, actions);
            ImGui::TextWrapped("%s", worker.text.c_str());
            break;
        case WorkerTuningState::Unreadable:
            ToolStatusBadge("Unreadable", ToolToneColor(ToolTone::Error));
            ImGui::TextWrapped("%s", worker.text.c_str());
            break;
        }
        return;
    }
    if (standalone)
        ImGui::TextDisabled("%s", snapshot.ready ? "The running scene does not read this file."
                                                 : "Open a scene that reads it to see changes live.");
}

void TuningInspector::Fields(const TuningFileState& file, const EditorSnapshot&, const EditorUiActions& actions,
                             const std::string& query, bool modifiedOnly) {
    std::vector<std::string> groups{std::string()}; // Fields without a group come first
    for (const auto& field : file.fields)
        if (std::ranges::find(groups, field.group) == groups.end()) groups.push_back(field.group);
    for (const auto& group : groups) {
        std::vector<const TuningField*> shown;
        for (const auto& field : file.fields)
            if (field.group == group && Matches(file, field, query, modifiedOnly)) shown.push_back(&field);
        if (shown.empty()) continue;
        if (!group.empty()) ImGui::SeparatorText(group.c_str());
        ImGui::PushID(group.c_str());
        float labelWidth = 0;
        for (const auto* field : shown)
            labelWidth = std::max(labelWidth, ImGui::CalcTextSize(PropertyLabel(field->name).c_str()).x);
        labelWidth += ImGui::GetFontSize();
        bool properties = false;
        for (const auto* field : shown) {
            const bool scalar = !field->table && !field->spec.list;
            // The table spans fields, so it opens and closes outside a field's ID scope.
            if (scalar && !properties) properties = BeginInspectorProperties("tuning", labelWidth);
            if (!scalar && properties) {
                EndInspectorProperties();
                properties = false;
            }
            ImGui::PushID(field->name.c_str());
            if (scalar && properties)
                Scalar(file, *field, actions);
            else if (field->table)
                Table(file, *field, actions);
            else if (!scalar)
                List(file, *field, actions);
            ImGui::PopID();
        }
        if (properties) EndInspectorProperties();
        ImGui::PopID();
    }
}

void TuningInspector::Scalar(const TuningFileState& file, const TuningField& field, const EditorUiActions& actions) {
    const TuningLocation at{field.name};
    const bool modified = Modified(file, field);
    InspectorRow row{PropertyLabel(field.name), Tooltip(file, field), modified, false, RevertTooltip};
    const bool previewing = _previewing && _previewing->source == file.source && _previewing->at == at;
    auto property = TuningPropertyValue(previewing ? _previewing->value : field.values.front());
    const auto edit = InspectorPropertyField(row, TuningPropertySpec(field.spec), property);
    if (edit.changed) {
        _previewing = Previewing{file.source, at, TuningValueOf(property)};
        actions.tuning({TuningEditKind::Preview, file.source, at, _previewing->value});
    }
    if (edit.reset && modified && file.CommittedField(field.name))
        actions.tuning({TuningEditKind::Revert, file.source, at});
    if (edit.context) OpenContext(file, field);
}

bool TuningInspector::Value(const TuningFileState& file, const TuningLocation& at, const TuningSpec& spec,
                            const TuningValue& stored, bool modified, float width, const EditorUiActions& actions) {
    const bool previewing = _previewing && _previewing->source == file.source && _previewing->at == at;
    auto property = TuningPropertyValue(previewing ? _previewing->value : stored);
    const auto propertySpec = TuningPropertySpec(spec);
    ImGui::PushID(static_cast<int>(at.element));
    ImGui::SetNextItemWidth(width);
    const bool number =
        spec.type != TuningType::Bool && spec.type != TuningType::Color && spec.type != TuningType::Choice;
    if (number) ImGui::PushFont(ToolFont(ToolFontRole::Technical), 0);
    if (modified) ImGui::PushStyleColor(ImGuiCol_Text, ToolToneColor(ToolTone::Warning));
    const bool changed = PropertyWidget(propertySpec, property);
    if (modified) ImGui::PopStyleColor();
    if (number) ImGui::PopFont();
    if (ImGui::IsItemHovered() && !ImGui::IsItemActive())
        ToolTooltip(
            (PropertyText(propertySpec, property) + (modified ? "\nModified since the last commit." : "")).c_str());
    ImGui::PopID();
    if (!changed) return false;
    _previewing = Previewing{file.source, at, TuningValueOf(property)};
    actions.tuning({TuningEditKind::Preview, file.source, at, _previewing->value});
    return true;
}

void TuningInspector::List(const TuningFileState& file, const TuningField& field, const EditorUiActions& actions) {
    const bool modified = Modified(file, field);
    ImGui::AlignTextToFramePadding();
    if (modified) ImGui::PushStyleColor(ImGuiCol_Text, ToolToneColor(ToolTone::Warning));
    ImGui::TextUnformatted(PropertyLabel(field.name).c_str());
    if (modified) ImGui::PopStyleColor();
    ToolTooltip(Tooltip(file, field).c_str());
    if (ImGui::IsItemHovered() && ImGui::IsMouseReleased(ImGuiMouseButton_Right)) OpenContext(file, field);
    const float buttons = ImGui::GetFrameHeight() * 2 + ImGui::GetStyle().ItemSpacing.x;
    ImGui::SameLine(std::max(ImGui::GetCursorPosX(), ImGui::GetContentRegionMax().x - buttons));
    if (ToolIconButton("add", "+", "Add a copy of the last element. A new length needs a build."))
        actions.tuning({TuningEditKind::Resize, file.source, {field.name}, {}, TuningResize::Append});
    ImGui::SameLine();
    ImGui::BeginDisabled(field.values.size() < 2);
    if (ToolIconButton("remove", "-", "Remove the last element. A new length needs a build."))
        actions.tuning({TuningEditKind::Resize,
                        file.source,
                        {field.name, 0, {}, field.values.size() - 1},
                        {},
                        TuningResize::Remove});
    ImGui::EndDisabled();
    const float width = CellWidth * ToolUiScale();
    const float right = ImGui::GetCursorScreenPos().x + ImGui::GetContentRegionAvail().x;
    for (std::size_t i = 0; i < field.values.size(); ++i) {
        const TuningLocation at{field.name, 0, {}, i};
        if (i > 0) {
            ImGui::SameLine();
            if (ImGui::GetCursorScreenPos().x + width > right) ImGui::NewLine();
        }
        (void)Value(file, at, field.spec, field.values[i], Modified(file, field, at), width, actions);
    }
}

void TuningInspector::Table(const TuningFileState& file, const TuningField& field, const EditorUiActions& actions) {
    const bool modified = Modified(file, field);
    ImGui::AlignTextToFramePadding();
    if (modified) ImGui::PushStyleColor(ImGuiCol_Text, ToolToneColor(ToolTone::Warning));
    ImGui::TextUnformatted(PropertyLabel(field.name).c_str());
    if (modified) ImGui::PopStyleColor();
    ToolTooltip(Tooltip(file, field).c_str());
    if (ImGui::IsItemHovered() && ImGui::IsMouseReleased(ImGuiMouseButton_Right)) OpenContext(file, field);
    ImGui::SameLine(std::max(ImGui::GetCursorPosX(), ImGui::GetContentRegionMax().x - ImGui::GetFrameHeight()));
    if (ToolIconButton("add-row", "+", "Add a copy of the last row. A new row count needs a build."))
        actions.tuning({TuningEditKind::Resize, file.source, {field.name}, {}, TuningResize::Append});
    constexpr auto flags = ImGuiTableFlags_Borders | ImGuiTableFlags_RowBg | ImGuiTableFlags_SizingFixedFit |
                           ImGuiTableFlags_ScrollX | ImGuiTableFlags_NoSavedSettings;
    const int columns = static_cast<int>(field.columns.size()) + 2;
    const auto& style = ImGui::GetStyle();
    const float cell = CellWidth * ToolUiScale();
    float width = ImGui::CalcTextSize("00").x + ImGui::GetFrameHeight() + style.CellPadding.x * 2 * columns;
    for (const auto& values : field.rows.front())
        width += (cell + style.ItemSpacing.x) * static_cast<float>(values.size());
    // A scrollbar takes room only when the columns do not fit.
    const float scrollbar = width > ImGui::GetContentRegionAvail().x ? style.ScrollbarSize : 0.0f;
    const float header = ImGui::GetTextLineHeight() + style.CellPadding.y * 2;
    const float height = header +
                         (ImGui::GetFrameHeight() + style.CellPadding.y * 2) * static_cast<float>(field.rows.size()) +
                         scrollbar + 2; // + the outer borders
    if (!ImGui::BeginTable("##table", columns, flags, {0, height})) return;
    ImGui::TableSetupColumn("#");
    for (const auto& column : field.columns) {
        const auto length = field.rows.front()[static_cast<std::size_t>(&column - field.columns.data())].size();
        ImGui::TableSetupColumn(PropertyLabel(column.name).c_str(), ImGuiTableColumnFlags_WidthFixed,
                                cell * static_cast<float>(length) +
                                    ImGui::GetStyle().ItemSpacing.x * static_cast<float>(length - 1));
    }
    ImGui::TableSetupColumn("##remove", ImGuiTableColumnFlags_WidthFixed, ImGui::GetFrameHeight());
    ImGui::TableHeadersRow();
    for (std::size_t c = 0; c < field.columns.size(); ++c) { // Column notes on the header
        ImGui::TableSetColumnIndex(static_cast<int>(c) + 1);
        const auto& column = field.columns[c];
        if (!column.note.empty() &&
            (ImGui::TableGetColumnFlags(static_cast<int>(c) + 1) & ImGuiTableColumnFlags_IsHovered))
            ToolTooltip(column.note.c_str());
    }
    for (std::size_t row = 0; row < field.rows.size(); ++row) {
        ImGui::PushID(static_cast<int>(row));
        ImGui::TableNextRow();
        ImGui::TableSetColumnIndex(0);
        ImGui::AlignTextToFramePadding();
        ImGui::Text("%zu", row);
        for (std::size_t c = 0; c < field.columns.size(); ++c) {
            ImGui::TableSetColumnIndex(static_cast<int>(c) + 1);
            const auto& column = field.columns[c];
            ImGui::PushID(column.name.c_str());
            for (std::size_t i = 0; i < field.rows[row][c].size(); ++i) {
                if (i > 0) ImGui::SameLine();
                const TuningLocation at{field.name, row, column.name, i};
                (void)Value(file, at, column.spec, field.rows[row][c][i], Modified(file, field, at), cell, actions);
            }
            ImGui::PopID();
        }
        ImGui::TableSetColumnIndex(columns - 1);
        ImGui::BeginDisabled(field.rows.size() < 2);
        if (ToolIconButton("remove-row", "x", "Remove this row. A new row count needs a build."))
            actions.tuning({TuningEditKind::Resize, file.source, {field.name, row}, {}, TuningResize::Remove});
        ImGui::EndDisabled();
        ImGui::PopID();
    }
    ImGui::EndTable();
}

void TuningInspector::OpenContext(const TuningFileState& file, const TuningField& field) {
    _contextFile = file.source;
    _contextField = field.name;
    ImGui::OpenPopup(_popup); // The file's popup, opened from inside a field's ID scope
}

void TuningInspector::ContextMenu(const TuningFileState& file, const EditorSnapshot& snapshot,
                                  const EditorUiActions& actions) {
    if (!ImGui::BeginPopup(ContextPopup)) return;
    const auto* field = _contextFile == file.source ? file.Field(_contextField) : nullptr;
    if (field) {
        ImGui::BeginDisabled(!Modified(file, *field) || !file.CommittedField(field->name));
        if (ImGui::MenuItem("Revert to committed"))
            actions.tuning({TuningEditKind::Revert, file.source, {field->name}});
        ImGui::EndDisabled();
        ToolTooltip("Write the field's values from the last commit (git HEAD) back into the file.");
        if (ImGui::MenuItem("Open in code editor")) actions.openSource(Utf8(file.source), field->line);
        if (ImGui::MenuItem("Send to Agent")) {
            auto details = Details(file, *field);
            std::ranges::replace(details, '\n', ' ');
            _agentText = TuningContext(file.relative, field->line, field->name, FieldText(*field), details);
        }
        ToolTooltip("Paste this field's file, line, value, bounds and note into the agent session.");
        ImGui::Separator();
    }
    const bool undo = snapshot.tuning && snapshot.tuning->CanUndo();
    const bool redo = snapshot.tuning && snapshot.tuning->CanRedo();
    if (ImGui::MenuItem("Undo tuning change", "Ctrl+Z", false, undo)) actions.tuning({TuningEditKind::Undo});
    if (ImGui::MenuItem("Redo tuning change", "Ctrl+Y", false, redo)) actions.tuning({TuningEditKind::Redo});
    ImGui::EndPopup();
}

void TuningInspector::Finish(const EditorUiActions& actions) {
    if (!_previewing || ImGui::IsAnyItemActive()) return;
    actions.tuning({TuningEditKind::Set, _previewing->source, _previewing->at, _previewing->value});
    _previewing.reset();
}

void TuningInspector::Shortcuts(const EditorSnapshot& snapshot, const EditorUiActions& actions) {
    const auto& io = ImGui::GetIO();
    if (!snapshot.tuning || !io.KeyCtrl || io.WantTextInput ||
        !ImGui::IsWindowFocused(ImGuiFocusedFlags_RootAndChildWindows))
        return;
    if (ImGui::IsKeyPressed(ImGuiKey_Z, false) && !io.KeyShift) {
        if (snapshot.tuning->CanUndo()) actions.tuning({TuningEditKind::Undo});
    } else if (ImGui::IsKeyPressed(ImGuiKey_Y, false) || (ImGui::IsKeyPressed(ImGuiKey_Z, false) && io.KeyShift)) {
        if (snapshot.tuning->CanRedo()) actions.tuning({TuningEditKind::Redo});
    }
}

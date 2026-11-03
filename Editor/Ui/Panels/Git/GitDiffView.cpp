#include "Editor/Ui/Panels/Git/GitDiffView.h"
#include <extras/IconsFontAwesome6.h>
#include <algorithm>
#include <format>
#include "Editor/Ui/Style/EditorTheme.h"
#include "ToolUi/ToolPixel.h"
#include "ToolUi/ToolTheme.h"
#include "ToolUi/ToolWidgets.h"

namespace {
constexpr ImGuiWindowFlags DiffFlags = ImGuiWindowFlags_NoDecoration | ImGuiWindowFlags_NoMove |
                                       ImGuiWindowFlags_NoSavedSettings | ImGuiWindowFlags_NoDocking |
                                       ImGuiWindowFlags_NoScrollbar | ImGuiWindowFlags_NoScrollWithMouse;
constexpr float Padding = 10.0f;
constexpr float SideGap = 8.0f;
constexpr float LineTint = 0.14f;
void TextDiff(const GitDiff& diff) {
    if (diff.lines.empty()) {
        ToolEmptyState("No text change", "The file's content is the same, or only its name or mode changed.");
        return;
    }
    const float scale = ToolUiScale();
    if (ImGui::BeginChild("lines", {0, 0}, 0, ImGuiWindowFlags_HorizontalScrollbar)) {
        ImGui::PushFont(ToolFont(ToolFontRole::Technical), ToolMetrics::BodySize);
        const float height = ImGui::GetTextLineHeight() + 2 * scale;
        const float width = std::max(ImGui::GetContentRegionAvail().x, 4000 * scale);
        auto* draw = ImGui::GetWindowDrawList();
        ImGuiListClipper clipper;
        clipper.Begin(static_cast<int>(diff.lines.size()), height);
        while (clipper.Step())
            for (int index = clipper.DisplayStart; index < clipper.DisplayEnd; ++index) {
                const auto& line = diff.lines[static_cast<std::size_t>(index)];
                const auto start = ImGui::GetCursorScreenPos();
                ImVec4 color = ToolPalette::Secondary;
                if (line.kind == GitDiffLineKind::Added || line.kind == GitDiffLineKind::Removed) {
                    const auto tone =
                        ToolToneColor(line.kind == GitDiffLineKind::Added ? ToolTone::Success : ToolTone::Error);
                    draw->AddRectFilled(start, {start.x + width, start.y + height},
                                        ImGui::GetColorU32(EditorTintSurface(ToolPalette::Content, tone, LineTint)));
                    color = ToolPalette::Text;
                } else if (line.kind == GitDiffLineKind::Hunk) {
                    color = EditorInfoColor();
                }
                ImGui::PushStyleColor(ImGuiCol_Text, color);
                ImGui::TextUnformatted(line.text.c_str());
                ImGui::PopStyleColor();
                ImGui::SetCursorScreenPos({start.x, start.y + height});
            }
        ImGui::Dummy({width, 0});
        if (diff.truncated) ImGui::TextColored(ToolPalette::Muted, "The change is longer than this view shows.");
        ImGui::PopFont();
    }
    ImGui::EndChild();
}
void Picture(const char* title, const std::filesystem::path& file, const EditorUiActions& actions, ImVec2 area) {
    if (ImGui::BeginChild(title, area)) {
        ImGui::TextColored(ToolPalette::Muted, "%s", title);
        const auto room = ImGui::GetContentRegionAvail();
        if (file.empty()) {
            ImGui::TextColored(ToolPalette::Muted, "No file on this side.");
        } else if (const auto image = actions.gitImage ? actions.gitImage(file) : EditorGitImage{}; !image.texture) {
            ImGui::TextColored(ToolPalette::Muted, "Could not load this picture.");
        } else {
            const float zoom =
                std::min(room.x / static_cast<float>(image.width), room.y / static_cast<float>(image.height));
            const ImVec2 size{static_cast<float>(image.width) * zoom, static_cast<float>(image.height) * zoom};
            const auto start = ImGui::GetCursorScreenPos();
            const ImVec2 from{start.x + (room.x - size.x) * 0.5f, start.y + (room.y - size.y) * 0.5f};
            ImGui::GetWindowDrawList()->AddImage(ImTextureID(image.texture), from, {from.x + size.x, from.y + size.y});
            ImGui::SetCursorScreenPos({from.x, from.y + size.y});
            ImGui::Dummy({0, 0});
        }
    }
    ImGui::EndChild();
}
void Sound(const char* title, const std::filesystem::path& file, const EditorUiActions& actions) {
    ImGui::BeginDisabled(file.empty());
    const auto label = std::format(ICON_FA_PLAY "  {}###{}", title, title);
    if (ToolButton(label.c_str()) && actions.previewAudio) {
        AudioPreviewRequest request;
        request.action = AudioPreviewAction::Play;
        request.file = file;
        actions.previewAudio(request);
    }
    ImGui::EndDisabled();
    if (file.empty()) ToolTooltip("No file on this side.");
}
} // namespace

void GitDiffView::Draw(const GitDiff& diff, const EditorUiActions& actions, ImVec2 position, ImVec2 size) const {
    const float scale = ToolUiScale();
    ImGui::SetNextWindowPos(position);
    ImGui::SetNextWindowSize(size);
    ImGui::PushStyleVar(ImGuiStyleVar_WindowPadding, {Padding * scale, Padding * scale});
    ImGui::PushStyleVar(ImGuiStyleVar_WindowBorderSize, 0.0f);
    ImGui::PushStyleColor(ImGuiCol_WindowBg, ToolPalette::Content);
    if (ImGui::Begin("Change###editor.git-diff", nullptr, DiffFlags)) {
        const float closeWidth = ImGui::GetFrameHeight();
        ImGui::AlignTextToFramePadding();
        ImGui::TextUnformatted(
            ToolClipMiddle(diff.path.c_str(), ImGui::GetContentRegionAvail().x - closeWidth * 1.5f).c_str());
        ImGui::SameLine(ImGui::GetContentRegionMax().x - closeWidth);
        if (ToolIconButton("git-diff-close", ICON_FA_XMARK, "Back to the scene  (Esc)") && actions.git)
            actions.git(GitRequest{.kind = GitRequestKind::CloseDiff});
        ImGui::Spacing();
        switch (diff.kind) {
        case GitDiffKind::None   : break;
        case GitDiffKind::Loading: ToolActivity(); break;
        case GitDiffKind::Text   : TextDiff(diff); break;
        case GitDiffKind::Image  : {
            const auto room = ImGui::GetContentRegionAvail();
            const float gap = SideGap * scale;
            const ImVec2 area{(room.x - gap) * 0.5f, room.y};
            Picture("Before", diff.before, actions, area);
            ImGui::SameLine(0, gap);
            Picture("After", diff.after, actions, area);
            break;
        }
        case GitDiffKind::Audio:
            ImGui::TextColored(ToolPalette::Secondary, "Listen to both versions.");
            ImGui::Spacing();
            Sound("Before", diff.before, actions);
            ImGui::SameLine();
            Sound("After", diff.after, actions);
            break;
        case GitDiffKind::Binary: ToolEmptyState("Binary file", "Its content cannot be compared as text."); break;
        case GitDiffKind::Folder:
            ToolEmptyState("New folder",
                           "Everything in it is new. Its files appear separately after the first commit.");
            break;
        }
    }
    ImGui::End();
    ImGui::PopStyleColor();
    ImGui::PopStyleVar(2);
}

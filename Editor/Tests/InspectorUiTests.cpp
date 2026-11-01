#include <catch2/catch_test_macros.hpp>
#include <imgui.h>
#include <array>
#include <memory>
#include "Editor/Ui/Panels/Inspector/InspectorPanel.h"
#include "Editor/Ui/Widgets/EditorWidgets.h"
#include "Editor/Ui/Widgets/InspectorWidgets.h"
#include "ToolUi/ToolTheme.h"
#include "ToolUi/ToolWidgets.h"

TEST_CASE("ToolUi.FontRoles.FollowTheRegisteredContext", "[engine-editor]") {
    const std::unique_ptr<ImGuiContext, decltype(&ImGui::DestroyContext)> editor(ImGui::CreateContext(),
                                                                                 ImGui::DestroyContext);
    auto* ui = ImGui::GetIO().Fonts->AddFontDefaultVector();
    auto* technical = ImGui::GetIO().Fonts->AddFontDefaultVector();
    RegisterToolFonts(ui, technical);
    CHECK(ToolFont(ToolFontRole::Ui) == ui);
    CHECK(ToolFont(ToolFontRole::Technical) == technical);
    {
        const std::unique_ptr<ImGuiContext, decltype(&ImGui::DestroyContext)> fixture(ImGui::CreateContext(),
                                                                                      ImGui::DestroyContext);
        ImGui::SetCurrentContext(fixture.get());
        auto& io = ImGui::GetIO();
        io.FontDefault = io.Fonts->AddFontDefaultVector();
        CHECK(ToolFont(ToolFontRole::Ui) == io.FontDefault);
        CHECK(ToolFont(ToolFontRole::Technical) == io.FontDefault);
        ClearToolFonts();
    }
    ImGui::SetCurrentContext(editor.get());
    CHECK(ToolFont(ToolFontRole::Technical) == technical);
    ClearToolFonts();
    CHECK(ToolFont(ToolFontRole::Technical) == ui);
}

TEST_CASE("Editor.Inspector.ResizesWithoutOverflowOrCommands", "[engine-editor]") {
    const std::unique_ptr<ImGuiContext, decltype(&ImGui::DestroyContext)> context(ImGui::CreateContext(),
                                                                                  ImGui::DestroyContext);
    auto& io = ImGui::GetIO();
    io.IniFilename = nullptr;
    io.LogFilename = nullptr;
    io.DisplaySize = {1600, 1600};
    io.FontDefault = io.Fonts->AddFontDefaultVector();
    unsigned char* pixels = nullptr;
    int atlasWidth = 0;
    int atlasHeight = 0;
    io.Fonts->GetTexDataAsRGBA32(&pixels, &atlasWidth, &atlasHeight);
    io.Fonts->SetTexID(ImTextureID{1});
    ApplyToolTheme();

    SceneDefinition definition;
    definition.id = "inspector";
    definition.description = "A scene with multiple property types and long labels.";
    definition.parameters = {
        {"count", "A long numeric property label", SceneParameterKind::Integer, std::int64_t{3}, 0, 10},
        {"seed", "Seed", SceneParameterKind::Integer, std::int64_t{42}, 0, 1000000},
        {"size", "Size", SceneParameterKind::Float, 0.5, 0, 1},
        {"enabled", "Enabled", SceneParameterKind::Boolean, true},
        {"color", "Tint", SceneParameterKind::Color, SceneColor{}},
        {"choice",
         "View",
         SceneParameterKind::Choice,
         std::string("A long selected value"),
         0,
         1,
         SceneApplyMode::OnReset,
         {"A long selected value", "Other"}}};
    auto buttons = definition.parameters.back();
    buttons.id = "buttons";
    buttons.label = "Type";
    buttons.choiceButtons = true;
    definition.parameters.push_back(buttons);
    SceneParameter phase{"phase", "Phase", SceneParameterKind::Choice, std::string("Enter")};
    phase.choices = {"Enter", "Phase 2", "Final"};
    phase.choiceButtons = true;
    phase.choiceButtonColumns = 3;
    phase.apply = SceneApplyMode::NextAction;
    definition.parameters.push_back(phase);
    definition.actions = {{"replay", "Replay"},
                          {"end", "End effects"},
                          {"gain", "Gain / refresh", "", true},
                          {"special", "Special action"},
                          {"combine", "Combine buffs"}};
    const std::vector catalog{definition};
    auto parameters = SceneDefaults(definition);
    parameters["count"] = std::int64_t{4};
    const auto before = parameters;
    const EditorDiagnostics diagnostics;
    const std::string status;
    EditorSnapshot snapshot{catalog, &catalog.front(),   parameters, {"count"}, diagnostics,
                            {},      EditorPhase::Ready, status};
    snapshot.ready = true;
    snapshot.inspection.disabledParameters = {"enabled"};
    snapshot.inspection.disabledActions = {"end"};
    snapshot.inspection.values = {
        {"Wall charges", "0", SceneReadoutKind::Number},
        {"State", "Playing", SceneReadoutKind::Status},
        {"Note", "A longer explanation that must stay readable when the Inspector is narrow."}};
    int commands = 0;
    EditorUiActions actions;
    actions.parameter = [&](const std::string&, const SceneValue&) { ++commands; };
    actions.action = [&](const std::string&) { ++commands; };
    InspectorPanel panel;
    for (const float scale : std::array{0.9375f, 1.25f, 2.5f}) {
        ImGui::GetStyle().FontScaleMain = scale;
        for (const float width : std::array{160.0f, 312.0f, 520.0f}) {
            for (const float height : std::array{420.0f, 1500.0f}) {
                CAPTURE(scale, width, height);
                float footerY = 0;
                // Resize, vertical-scrollbar appearance, then settled content extents.
                for (int frame = 0; frame < 3; ++frame) {
                    ImGui::NewFrame();
                    ImGui::SetNextWindowPos({0, 0});
                    ImGui::SetNextWindowSize({width, height});
                    ImGui::Begin("Inspector", nullptr, ImGuiWindowFlags_NoCollapse);
                    panel.Draw(snapshot, actions);
                    if (frame == 1) {
                        footerY = ImGui::GetItemRectMin().y;
                        io.AddMousePosEvent(width * 0.5f, footerY - ImGui::GetFrameHeight() * 2);
                        io.AddMouseWheelEvent(0, -10);
                    }
                    if (frame == 2) {
                        CHECK(ImGui::GetScrollMaxX() == 0);
                        CHECK(ImGui::GetScrollMaxY() == 0);
                        // The last item is the authored primary action, outside the scrolled child.
                        CHECK(ImGui::GetItemRectMin().y == footerY);
                        CHECK(ImGui::GetItemRectMin().y >= ImGui::GetWindowPos().y);
                        CHECK(ImGui::GetItemRectMax().y <= ImGui::GetWindowPos().y + ImGui::GetWindowSize().y);
                    }
                    ImGui::End();
                    ImGui::Render();
                }
            }
        }
    }
    CHECK(commands == 0);
    CHECK(parameters == before);
}

TEST_CASE("Editor.Inspector.NarrowControlsKeepReadableHeight", "[engine-editor]") {
    const std::unique_ptr<ImGuiContext, decltype(&ImGui::DestroyContext)> context(ImGui::CreateContext(),
                                                                                  ImGui::DestroyContext);
    auto& io = ImGui::GetIO();
    io.IniFilename = nullptr;
    io.LogFilename = nullptr;
    io.DisplaySize = {1200, 1600};
    io.FontDefault = io.Fonts->AddFontDefaultVector();
    unsigned char* pixels = nullptr;
    int atlasWidth = 0, atlasHeight = 0;
    io.Fonts->GetTexDataAsRGBA32(&pixels, &atlasWidth, &atlasHeight);
    io.Fonts->SetTexID(ImTextureID{1});
    ApplyToolTheme();
    SceneParameter choices{"phase", "Phase", SceneParameterKind::Choice, std::string("First phase")};
    choices.choices = {"First phase", "Second phase", "Final phase"};
    choices.choiceButtons = true;
    choices.choiceButtonColumns = 3;
    auto value = choices.initial;
    const std::array actions{SceneAction{"start", "Start the selected phase again", "", true}};
    int commands = 0;
    for (const float scale : std::array{0.9375f, 1.25f, 2.5f}) {
        ImGui::GetStyle().FontScaleMain = scale;
        for (const float width : std::array{160.0f, 320.0f}) {
            CAPTURE(scale, width);
            ImGui::NewFrame();
            ImGui::SetNextWindowSize({width, 1500});
            ImGui::Begin("Narrow controls", nullptr, ImGuiWindowFlags_NoCollapse);
            BeginInspectorStyle();
            // Single-line controls select the shared body font independently of the Inspector's font.
            ImGui::PushFont(ToolFont(ToolFontRole::Medium), ToolMetrics::BodySize);
            const float line = ImGui::GetFrameHeightWithSpacing();
            ImGui::PopFont();
            CHECK_FALSE(ToolSingleLineButton("A long action label that cannot fit in one narrow column"));
            CHECK(ImGui::GetItemRectSize().y <= line);
            CHECK_FALSE(EditorParameterWidget(choices, value));
            // Three choices may occupy three rows; individual labels never add extra rows.
            CHECK(ImGui::GetItemRectSize().y <=
                  line * static_cast<float>(choices.choices.size()) + ImGui::GetStyle().CellPadding.y * 2);
            const float before = ImGui::GetCursorPosY();
            InspectorActionGroup(actions, {}, [&](const std::string&) { ++commands; });
            CHECK(ImGui::GetCursorPosY() - before <= line);
            InspectorSummary("A long scene title",
                             "A long description with sufficient words to wrap through many lines in a narrow panel.");
            CHECK(ImGui::GetItemRectSize().y <= ImGui::GetTextLineHeight() * 2);
            EndInspectorStyle();
            ImGui::End();
            ImGui::Render();
        }
    }
    CHECK(commands == 0);
    CHECK(value == choices.initial);
}

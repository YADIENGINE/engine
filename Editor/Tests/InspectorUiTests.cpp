#include <catch2/catch_test_macros.hpp>
#include <imgui.h>
#include <array>
#include <memory>
#include "Editor/Ui/Panels/Inspector/InspectorPanel.h"
#include "Editor/Ui/Style/EditorTheme.h"

TEST_CASE("Editor.Inspector.ResizesWithoutOverflowOrCommands", "[engine-editor]") {
    const std::unique_ptr<ImGuiContext, decltype(&ImGui::DestroyContext)> context(ImGui::CreateContext(),
                                                                                  ImGui::DestroyContext);
    auto& io = ImGui::GetIO();
    io.IniFilename = nullptr;
    io.LogFilename = nullptr;
    io.DisplaySize = {1600, 1600};
    io.Fonts->AddFontDefaultVector();
    io.Fonts->AddFontDefaultVector();
    unsigned char* pixels = nullptr;
    int atlasWidth = 0;
    int atlasHeight = 0;
    io.Fonts->GetTexDataAsRGBA32(&pixels, &atlasWidth, &atlasHeight);
    io.Fonts->SetTexID(ImTextureID{1});
    ApplyEditorTheme();

    PreviewDefinition definition;
    definition.id = "inspector";
    definition.description = "A preview with multiple property types and long labels.";
    definition.parameters = {
        {"count", "A long numeric property label", PreviewParameterKind::Integer, std::int64_t{3}, 0, 10},
        {"seed", "Seed", PreviewParameterKind::Integer, std::int64_t{42}, 0, 1000000},
        {"size", "Size", PreviewParameterKind::Float, 0.5, 0, 1},
        {"enabled", "Enabled", PreviewParameterKind::Boolean, true},
        {"color", "Tint", PreviewParameterKind::Color, PreviewColor{}},
        {"choice",
         "View",
         PreviewParameterKind::Choice,
         std::string("A long selected value"),
         0,
         1,
         PreviewApplyMode::OnReset,
         {"A long selected value", "Other"}}};
    auto buttons = definition.parameters.back();
    buttons.id = "buttons";
    buttons.label = "Type";
    buttons.choiceButtons = true;
    definition.parameters.push_back(buttons);
    definition.actions = {{"replay", "Replay"},
                          {"end", "End effects"},
                          {"gain", "Gain / refresh", "", true},
                          {"special", "Special action"},
                          {"combine", "Combine buffs"}};
    const std::vector catalog{definition};
    auto parameters = PreviewDefaults(definition);
    parameters["count"] = std::int64_t{4};
    const auto before = parameters;
    const EditorDiagnostics diagnostics;
    const std::string status;
    EditorSnapshot snapshot{catalog, &catalog.front(),   parameters, {"count"}, diagnostics,
                            {},      EditorPhase::Ready, status};
    snapshot.ready = true;
    snapshot.inspection.disabledParameters = {"enabled"};
    snapshot.inspection.disabledActions = {"end"};
    snapshot.inspection.values = {{"Wall charges", "0"}, {"Technical identifier", "a-long-technical-identifier"}};
    int commands = 0;
    EditorUiActions actions;
    actions.parameter = [&](const std::string&, const PreviewValue&) { ++commands; };
    actions.action = [&](const std::string&) { ++commands; };
    const InspectorPanel panel;
    for (const float scale : std::array{0.75f, 1.0f, 2.0f}) {
        ImGui::GetStyle().FontScaleMain = scale;
        for (const float width : std::array{160.0f, 312.0f, 520.0f}) {
            CAPTURE(scale, width);
            // Resize, vertical-scrollbar appearance, then settled content extents.
            for (int frame = 0; frame < 3; ++frame) {
                ImGui::NewFrame();
                ImGui::SetNextWindowPos({0, 0});
                ImGui::SetNextWindowSize({width, 1500});
                ImGui::Begin("Inspector", nullptr, ImGuiWindowFlags_NoCollapse);
                panel.Draw(snapshot, actions);
                if (frame == 2) CHECK(ImGui::GetScrollMaxX() == 0);
                ImGui::End();
                ImGui::Render();
            }
        }
    }
    CHECK(commands == 0);
    CHECK(parameters == before);
}

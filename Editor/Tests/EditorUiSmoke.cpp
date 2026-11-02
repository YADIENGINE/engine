// Opt-in look at the editor's real panels: a hidden window draws EditorWorkspace with made-up scenes and writes one PNG
// per stage. No worker, project or user settings are involved. Not a CTest: it needs a desktop/GPU.
//   EngineEditorUiSmoke <snapshot-folder> [viewport-image.png]
#include <imgui_internal.h>
#include <raylib.h>
#include <rlgl.h>
#include <array>
#include <filesystem>
#include <iostream>
#include <string>
#include <vector>
#include "Editor/Ui/Workspace/EditorWorkspace.h"
#include "Runtime/Game.h"
#include "ToolUi/ToolTheme.h"
#include "ToolUi/ToolUiContext.h"
#include "ToolUi/ToolWindow.h"

namespace {
enum class SmokeView { Idle, Running, Paused, Building, Inspector };
struct SmokeStage {
    const char* name;
    SmokeView view;
    int width = 1600;
    int height = 900;
    int scale = 100;
};
constexpr std::array Stages{
    SmokeStage{"idle", SmokeView::Idle},
    SmokeStage{"running", SmokeView::Running},
    SmokeStage{"paused", SmokeView::Paused},
    SmokeStage{"building", SmokeView::Building},
    SmokeStage{"narrow", SmokeView::Running, 1100, 680},
    SmokeStage{"narrow-150", SmokeView::Running, 1280, 760, 150},
    SmokeStage{"inspector-200", SmokeView::Inspector, 1600, 900, 200},
};
constexpr int FramesPerStage = 8;

GameSettings SmokeSettings() {
    WindowSettings window;
    window.title = "Editor UI review";
    window.width = Stages.front().width;
    window.height = Stages.front().height;
    window.visible = false;
    window.alwaysRun = true;
    window.targetFps = 60;
    return ToolGameSettings("editor-ui-review", window);
}

SceneParameter Parameter(std::string id, std::string label, SceneParameterKind kind, SceneValue initial,
                         std::string group) {
    SceneParameter parameter;
    parameter.id = std::move(id);
    parameter.label = std::move(label);
    parameter.kind = kind;
    parameter.initial = std::move(initial);
    parameter.group = std::move(group);
    return parameter;
}

std::vector<SceneDefinition> SampleCatalog() {
    SceneDefinition fight;
    fight.id = "sample.fight";
    fight.name = "Enemy fight";
    fight.category = "Fights";
    fight.description = "Play one enemy from the start of a stage.";
    auto enemy = Parameter("enemy", "Enemy", SceneParameterKind::Choice, std::string("Ice Dragon"), "Fight");
    enemy.choices = {"Slime", "Fire Dragon", "Ice Dragon"};
    auto stage = Parameter("stage", "Stage", SceneParameterKind::Choice, std::string("I"), "Fight");
    stage.choices = {"I", "II", "F"};
    stage.choiceButtons = true;
    stage.choiceButtonColumns = 3;
    auto level = Parameter("level", "Level", SceneParameterKind::Integer, std::int64_t{4}, "Fight");
    level.minimum = 1;
    level.maximum = 12;
    auto speed = Parameter("speed", "Time scale", SceneParameterKind::Float, 1.0, "Debug");
    speed.minimum = 0.1;
    speed.maximum = 2.0;
    auto hitboxes = Parameter("hitboxes", "Show hitboxes", SceneParameterKind::Boolean, false, "Debug");
    auto tint = Parameter("tint", "Arena tint", SceneParameterKind::Color, SceneColor{0.3f, 0.5f, 0.7f, 1.0f}, "Debug");
    fight.parameters = {enemy, stage, level, speed, hitboxes, tint};
    fight.actions = {SceneAction{"start", "Start", "Start the fight from this stage.", true},
                     SceneAction{"win", "Skip to the end", "Finish the fight.", false}};
    std::vector<SceneDefinition> catalog{fight};
    for (const char* name : {"Boss intro", "Boss finale"}) {
        SceneDefinition scene;
        scene.id = std::string("sample.") + name;
        scene.name = name;
        scene.category = "Fights";
        catalog.push_back(scene);
    }
    for (const char* category : {"Visuals", "Interface", "Mechanics", "Profiler", "World"}) {
        SceneDefinition scene;
        scene.id = std::string("sample.category.") + category;
        scene.name = std::string(category) + " gallery";
        scene.category = category;
        catalog.push_back(scene);
    }
    return catalog;
}
EditorCapabilities Capabilities(EditorPhase phase) {
    EditorCapabilities capabilities;
    const auto allow = [&](EditorCommand command) { capabilities.enabled[static_cast<std::size_t>(command)] = true; };
    capabilities.disabledReasons.fill("");
    switch (phase) {
    case EditorPhase::Ready:
    case EditorPhase::Paused:
        allow(EditorCommand::Stop);
        allow(EditorCommand::Pause);
        allow(EditorCommand::Reset);
        allow(EditorCommand::Refresh);
        allow(EditorCommand::BuildAndPlay);
        if (phase == EditorPhase::Paused) allow(EditorCommand::Step);
        capabilities.playbackEnabled = true;
        break;
    case EditorPhase::Building: allow(EditorCommand::Cancel); break;
    case EditorPhase::ReadingCatalog:
    case EditorPhase::Starting:
    case EditorPhase::Stopped:
    case EditorPhase::Error:
        allow(EditorCommand::Open);
        allow(EditorCommand::Refresh);
        allow(EditorCommand::BuildAndPlay);
        break;
    }
    return capabilities;
}

class EditorUiSmoke final : public Game {
public:
    EditorUiSmoke(std::filesystem::path output, const std::filesystem::path& image)
        : Game(SmokeSettings())
        , _output(std::move(output))
        , _catalog(SampleCatalog())
        , _parameters(SceneDefaults(_catalog.front()))
        , _ui(ToolUiOptions{.docking = true}) {
        _parameters["level"] = std::int64_t{7};
        _status = "Scene running";
        _diagnostics.Append("Build finished, no warnings");
        if (!image.empty()) _frame = LoadTexture(image.string().c_str());
        _preferences.Visible(EditorPanel::Console) = true;
    }
    ~EditorUiSmoke() override {
        if (_frame.id != 0) UnloadTexture(_frame);
    }
    [[nodiscard]] int Failures() const { return _failures; }

protected:
    void Update(float) override {
        if (_frameIndex == FramesPerStage) {
            ++_stage;
            _frameIndex = 0;
        }
        if (_stage == Stages.size()) {
            RequestQuit();
            return;
        }
        if (_frameIndex == 0) SetWindowSize({Stages[_stage].width, Stages[_stage].height});
        ++_frameIndex;
    }
    void DrawToolOverlay() const override {
        if (_stage == Stages.size()) return;
        const auto& stage = Stages[_stage];
        const auto phase = stage.view == SmokeView::Idle       ? EditorPhase::Stopped
                           : stage.view == SmokeView::Paused   ? EditorPhase::Paused
                           : stage.view == SmokeView::Building ? EditorPhase::Building
                                                               : EditorPhase::Ready;
        const bool idle = stage.view == SmokeView::Idle;
        const auto status = std::string(idle ? "Catalog ready" : _status);
        EditorSnapshot snapshot{
            _catalog, idle ? nullptr : &_catalog.front(), _parameters, {}, _diagnostics, Capabilities(phase), phase,
            status};
        snapshot.ready = phase == EditorPhase::Ready || phase == EditorPhase::Paused;
        snapshot.paused = phase == EditorPhase::Paused;
        snapshot.canvas = {1600, 900};
        snapshot.generation = 1;
        snapshot.streamFps = 60.0;
        snapshot.inspection.values = {SceneReadout{"Boss HP", "100%", SceneReadoutKind::Number},
                                      SceneReadout{"Time", "12.4 s", SceneReadoutKind::Number}};
        const ViewportImage image{_frame.id, _frame.width, _frame.height, 1, 1, 0.0};
        _ui.BeginFrame(stage.scale);
        (void)_workspace.Draw(snapshot, {}, _frame.id != 0 ? image : ViewportImage{}, _preferences, _fontSize);
        if (_frameIndex == FramesPerStage) {
            for (const auto* name : {"Toolbar###editor.toolbar", "Status###editor.status"}) {
                const auto* window = ImGui::FindWindowByName(name);
                if (window && window->ScrollMax.x > 1.0f) {
                    std::cerr << stage.name << ": horizontal overflow in " << name << '\n';
                    ++_failures;
                }
            }
        }
        _ui.EndFrame();
        if (_frameIndex == FramesPerStage) {
            rlDrawRenderBatchActive();
            const auto file = _output / (std::string(stage.name) + ".png");
            const Image shot = LoadImageFromScreen();
            if (!ExportImage(shot, file.string().c_str())) ++_failures;
            UnloadImage(shot);
            std::cout << "Snapshot: " << file.string() << '\n';
        }
    }

private:
    std::filesystem::path _output;
    std::vector<SceneDefinition> _catalog;
    SceneParameters _parameters;
    EditorDiagnostics _diagnostics;
    std::string _status;
    Texture2D _frame{};
    mutable EditorPreferences _preferences;
    mutable EditorWorkspace _workspace;
    mutable int _fontSize = 100;
    mutable int _failures = 0;
    mutable ToolUiContext _ui;
    std::size_t _stage = 0;
    int _frameIndex = 0;
};
} // namespace

int main(int argc, char** argv) {
    if (argc < 2) {
        std::cerr << "Usage: EngineEditorUiSmoke <snapshot-folder> [viewport-image.png]\n";
        return 2;
    }
    const auto output = std::filesystem::absolute(argv[1]);
    std::filesystem::create_directories(output);
    EditorUiSmoke smoke(output, argc > 2 ? std::filesystem::path(argv[2]) : std::filesystem::path{});
    smoke.Run();
    std::cout << "UI checks: " << smoke.Failures() << " failures\n";
    return smoke.Failures() == 0 ? 0 : 1;
}

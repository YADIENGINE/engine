// Opt-in look at the editor's real panels: a hidden window draws EditorWorkspace with made-up scenes and writes one PNG
// per stage. No worker, project or user settings are involved. Not a CTest: it needs a desktop/GPU.
//   EngineEditorUiSmoke <snapshot-folder> [viewport-image.png]
#include <imgui_internal.h>
#include <raylib.h>
#include <rlgl.h>
#include <algorithm>
#include <array>
#include <cmath>
#include <chrono>
#include <ctime>
#include <deque>
#include <filesystem>
#include <fstream>
#include <iostream>
#include <memory>
#include <optional>
#include <string>
#include <vector>
#include "Editor/Internals/Session/TuningWorkspace.h"
#include "Editor/Ui/Workspace/EditorWorkspace.h"
#include "Runtime/Game.h"
#include "ToolUi/ToolTheme.h"
#include "ToolUi/ToolUiContext.h"
#include "ToolUi/ToolWindow.h"

namespace {
enum class SmokeView { Idle, Running, Paused, Building, Inspector, QuickOpen, Palette, BuildFailed, AgentStrip };
struct SmokeStage {
    const char* name;
    SmokeView view;
    int width = 1600;
    int height = 900;
    int scale = 100;
    std::optional<EditorPanel> panel = std::nullopt; // Brought to the front of its dock
};
constexpr std::array Stages{
    SmokeStage{"idle", SmokeView::Idle},
    SmokeStage{"running", SmokeView::Running},
    SmokeStage{"paused", SmokeView::Paused},
    SmokeStage{"building", SmokeView::Building},
    SmokeStage{"narrow", SmokeView::Running, 1100, 680},
    SmokeStage{"narrow-150", SmokeView::Running, 1280, 760, 150},
    SmokeStage{"inspector-200", SmokeView::Inspector, 1600, 900, 200},
    SmokeStage{"output", SmokeView::Running, 1600, 900, 100, EditorPanel::Output},
    SmokeStage{"output-narrow", SmokeView::Running, 1100, 680, 100, EditorPanel::Output},
    SmokeStage{"profiler", SmokeView::Running, 1600, 1200, 100, EditorPanel::Profiler},
    SmokeStage{"tuning", SmokeView::Running, 1600, 900, 100, EditorPanel::Tuning},
    SmokeStage{"assets", SmokeView::Running, 1600, 900, 100, EditorPanel::Assets},
    SmokeStage{"audio", SmokeView::Running, 1600, 900, 100, EditorPanel::AudioPreview},
    SmokeStage{"agent-strip", SmokeView::AgentStrip},
    SmokeStage{"quick-open", SmokeView::QuickOpen},
    SmokeStage{"palette", SmokeView::Palette},
    SmokeStage{"build-failed", SmokeView::BuildFailed},
};
constexpr int FramesPerStage = 8;

void WriteFile(const std::filesystem::path& file, std::string_view text = "x") {
    std::filesystem::create_directories(file.parent_path());
    std::ofstream(file, std::ios::binary) << text;
}

// Brings a docked panel's tab forward; a no-op until the panel has a tab.
void SelectTab(const char* window) {
    auto* found = ImGui::FindWindowByName(window);
    if (found && found->DockNode && found->DockNode->TabBar) found->DockNode->TabBar->NextSelectedTabId = found->TabId;
}

EditorUiActions QuietActions() {
    EditorUiActions actions;
    actions.command = [](EditorCommand) {};
    actions.select = [](const std::string&) {};
    actions.parameter = [](const std::string&, const SceneValue&) {};
    actions.action = [](const std::string&) {};
    actions.clearOutput = [] {};
    actions.acknowledgeOutput = [] {};
    actions.playback = [](double, bool) {};
    actions.profiler = [](EditorProfilerCommand, Profiler::Recipe, const std::string&) {};
    actions.openOutputLog = [](bool) {};
    actions.tune = [](std::size_t, const TuningLocation&, const TuningValue&) {};
    actions.previewAudio = [](const AudioPreviewRequest&) {};
    actions.openPath = [](const std::filesystem::path&) {};
    actions.assetThumbnail = [](const std::string&) { return std::uint64_t{0}; };
    actions.assetCommand = [](const std::vector<std::string>&) {};
    return actions;
}

// A made-up live capture: steady frames with a few spikes, under three scopes.
std::shared_ptr<const Profiler::Capture> SampleCapture() {
    auto capture = std::make_shared<Profiler::Capture>();
    capture->scopes = {"Update", "Draw", "Fx"};
    Profiler::Run run;
    for (std::uint64_t index = 0; index < 180; ++index) {
        const double spike = index % 47 == 0 ? 9.0 : 0.0;
        Profiler::FrameSample frame;
        frame.id = index;
        frame.intervalMs = 15.5 + std::sin(static_cast<double>(index) * 0.2) * 1.5 + spike;
        frame.scopes = {{0, 4.2 + spike * 0.5, 3.9}, {1, 7.1, 5.1}, {2, 2.0 + spike * 0.5, 2.0}};
        run.frames.push_back(std::move(frame));
    }
    capture->runs.push_back(std::move(run));
    return capture;
}

std::shared_ptr<const Profiler::Summary> SampleSummary() {
    auto summary = std::make_shared<Profiler::Summary>();
    summary->fps = 60.0;
    summary->frame = {180, 16.2, 16.0, 18.4, 24.2, 25.0};
    summary->scopes = {{"Update", {180, 4.4, 4.2, 6.1, 8.7, 8.7}, {}},
                       {"Draw", {180, 7.1, 7.0, 7.6, 8.0, 8.1}, {}},
                       {"Fx", {180, 2.2, 2.0, 3.9, 6.5, 6.5}, {}}};
    return summary;
}

std::deque<EditorControlEvent> SampleEvents() {
    const auto now = std::chrono::system_clock::now();
    return {{now, "Opened Enemy fight", {}, {}, {}, {}},
            {now, "level: 4 \xE2\x86\x92 7", "sample.fight", "level", SceneValue{std::int64_t{4}}, {}},
            {now, "Trying the fight at a higher level", {}, {}, {}, {}},
            {now, "Screenshot shot-1230.png", {}, {}, {}, "shot-1230.png"}};
}

AudioWaveform SampleWaveform() {
    AudioWaveform waveform;
    waveform.sampleRate = 44100;
    waveform.channels = 2;
    waveform.seconds = 2.4f;
    for (int index = 0; index < 1024; ++index) {
        const float t = static_cast<float>(index) / 1024.0f;
        const float envelope = std::exp(-3.0f * t) * (0.6f + 0.4f * std::sin(t * 90.0f));
        waveform.peaks.push_back(std::abs(envelope));
        waveform.rms.push_back(std::abs(envelope) * 0.55f);
    }
    return waveform;
}

// Files for the Assets, Audio Preview and Tuning panels, in a folder of their own.
class SmokeProject {
public:
    SmokeProject() {
        std::error_code error;
        _root = std::filesystem::temp_directory_path(error) / ("editor-ui-smoke-" + std::to_string(::time(nullptr)));
        WriteFile(EngineDirectory() / "Assets" / "Graphics" / "Icons" / "Icons.asset.cpp");
        WriteFile(EngineDirectory() / "Assets" / "Graphics" / "Backdrops" / "Stars.asset.cpp");
        for (const char* file :
             {"Graphics/Enemies/Slime.asset.cpp", "Graphics/Enemies/Dragon.asset.cpp", "Graphics/Player/Hero.asset.cpp",
              "Audio/SE/Hit.wav", "Audio/SE/Pickup.wav", "Audio/SE/Explosion.wav", "Audio/Music/Theme.ogg"})
            WriteFile(AssetsDirectory() / file);
        const auto tuning = _root / "Game" / "Enemy.tuning.json";
        WriteFile(tuning, "{\n  \"radius\": { \"value\": 12, \"min\": 0, \"max\": 40 },\n"
                          "  \"speed\": { \"value\": 2.5, \"min\": 0, \"max\": 10 },\n"
                          "  \"visible\": { \"value\": true }\n}\n");
        WriteFile(_root / "build" / "Tuning" / "TuningFiles.json",
                  "{ \"version\": 1, \"files\": [ { \"target\": \"MyGame\", \"source\": \"" + tuning.generic_string() +
                      "\" } ] }");
    }
    ~SmokeProject() {
        std::error_code error;
        std::filesystem::remove_all(_root, error);
    }
    [[nodiscard]] std::filesystem::path EngineDirectory() const { return _root / "Engine"; }
    [[nodiscard]] std::filesystem::path AssetsDirectory() const { return _root / "Game" / "Assets"; }
    [[nodiscard]] std::filesystem::path BuildDirectory() const { return _root / "build"; }

private:
    std::filesystem::path _root;

public:
    SmokeProject(const SmokeProject&) = delete;
    SmokeProject& operator=(const SmokeProject&) = delete;
};

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
        FailBuild();
        _diagnostics.Acknowledge();
        if (!image.empty()) _frame = LoadTexture(image.string().c_str());
        _preferences.Visible(EditorPanel::Console) = true;
        _preferences.recent = {"sample.fight", "sample.Boss intro", "sample.category.Visuals"};
        _workspace.SetAssetRoots(_project.EngineDirectory(), _project.AssetsDirectory());
        _tuning.Open(_project.BuildDirectory());
    }
    ~EditorUiSmoke() override {
        if (_frame.id != 0) UnloadTexture(_frame);
    }
    [[nodiscard]] int Failures() const { return _failures; }

private:
    void FailBuild() const {
        _diagnostics.BeginAttempt();
        _diagnostics.Append("[12/48] Building Enemy.cpp", EditorLogLevel::Info, EditorLogSource::Build);
        _diagnostics.Append("Enemy.cpp(88): warning C4100: 'delta': unreferenced parameter", EditorLogLevel::Warning,
                            EditorLogSource::Build);
        _diagnostics.Append("Game/Enemies/Enemy.cpp(142): error C2065: 'cone': undeclared identifier",
                            EditorLogLevel::Error, EditorLogSource::Build);
        _diagnostics.Append("Enemy.obj : error LNK2019: unresolved external symbol \"void __cdecl Spawn(void)\"",
                            EditorLogLevel::Error, EditorLogSource::Build);
        _diagnostics.BuildExited(1);
        _diagnostics.FinishAttempt(EditorBuildResult::BuildFailed);
    }

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
        _tuning.Poll();
    }
    void DrawToolOverlay() const override {
        if (_stage == Stages.size()) return;
        const auto& stage = Stages[_stage];
        // A new failure, so the workspace reacts to it as it does to one the developer started.
        if (stage.view == SmokeView::BuildFailed && _frameIndex == 2) FailBuild();
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
        snapshot.profiler.liveEnabled = true;
        snapshot.profiler.live = _capture;
        snapshot.profiler.liveSummary = _summary;
        snapshot.tuning = &_tuning;
        snapshot.audio.file = _project.AssetsDirectory() / "Audio" / "SE" / "Hit.wav";
        snapshot.audio.length = _waveform->seconds;
        snapshot.audio.position = 0.7f;
        snapshot.audio.waveform = {WaveformState::Ready, _waveform, {}};
        snapshot.controlEvents = &_events;
        snapshot.controlEventCount = _events.size();
        // A failure reported through the control endpoint opens no popup; only the one stage wants it.
        snapshot.controlAttempt = stage.view == SmokeView::BuildFailed ? 0 : _diagnostics.Attempt().id;
        const ViewportImage image{_frame.id, _frame.width, _frame.height, 1, 1, 0.0};
        // Escape closes what the last stage opened, then the keys that open this stage's palette.
        auto& io = ImGui::GetIO();
        const bool floating = stage.view == SmokeView::QuickOpen || stage.view == SmokeView::Palette ||
                              stage.view == SmokeView::BuildFailed;
        if (floating && _frameIndex <= 2) io.AddKeyEvent(ImGuiKey_Escape, _frameIndex == 1);
        if ((stage.view == SmokeView::QuickOpen || stage.view == SmokeView::Palette) && _frameIndex >= 3 &&
            _frameIndex <= 4) {
            io.AddKeyEvent(ImGuiMod_Ctrl, _frameIndex == 3);
            if (stage.view == SmokeView::Palette) io.AddKeyEvent(ImGuiMod_Shift, _frameIndex == 3);
            io.AddKeyEvent(ImGuiKey_P, _frameIndex == 3);
        }
        _ui.BeginFrame(stage.scale);
        if (stage.panel) {
            _preferences.Visible(*stage.panel) = true;
            SelectTab(EditorPanelInfo(*stage.panel).window);
        }
        (void)_workspace.Draw(snapshot, _actions, _frame.id != 0 ? image : ViewportImage{}, _preferences, _fontSize);
        if (stage.view == SmokeView::AgentStrip) DrawAgentStrips(snapshot);
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
    // Each state at each width over the workspace (no Console session runs here), then the last one's list.
    void DrawAgentStrips(const EditorSnapshot& snapshot) const {
        constexpr std::array States{SessionState::Idle, SessionState::Working, SessionState::Waiting,
                                    SessionState::Done};
        constexpr std::array Widths{EditorAgentActivityMetrics::Ideal, EditorAgentActivityMetrics::Medium,
                                    EditorAgentActivityMetrics::Compact};
        ImGui::SetNextWindowPos({340, 130});
        ImGui::SetNextWindowSize({860, 620});
        ImGui::Begin("Agent strips", nullptr, ImGuiWindowFlags_NoDecoration | ImGuiWindowFlags_NoSavedSettings);
        std::size_t strip = 0;
        for (const auto state : States) {
            for (const float width : Widths) {
                EditorAgentStatus agent{.session = true, .title = "Claude", .state = state};
                if (state == SessionState::Working) agent.activity = "Edit EditorAgentActivity.cpp";
                ImGui::PushID(static_cast<int>(strip));
                if (_frameIndex == 1 && strip + 1 == _agentStrips.size()) _agentStrips[strip].Show();
                _agentStrips[strip++].Draw(snapshot, _actions, agent, width);
                ImGui::PopID();
                ImGui::SameLine();
            }
            ImGui::NewLine();
        }
        ImGui::End();
    }

    std::filesystem::path _output;
    std::vector<SceneDefinition> _catalog;
    SceneParameters _parameters;
    mutable EditorDiagnostics _diagnostics;
    std::string _status;
    SmokeProject _project;
    TuningWorkspace _tuning;
    EditorUiActions _actions = QuietActions();
    std::shared_ptr<const Profiler::Capture> _capture = SampleCapture();
    std::shared_ptr<const Profiler::Summary> _summary = SampleSummary();
    std::shared_ptr<const AudioWaveform> _waveform = std::make_shared<AudioWaveform>(SampleWaveform());
    std::deque<EditorControlEvent> _events = SampleEvents();
    Texture2D _frame{};
    mutable EditorPreferences _preferences;
    mutable EditorWorkspace _workspace;
    mutable std::array<EditorAgentActivity, 12> _agentStrips;
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

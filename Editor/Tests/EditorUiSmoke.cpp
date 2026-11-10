// Opt-in look at the editor's real panels: a hidden window draws EditorWorkspace with made-up scenes and writes one PNG
// per stage. No worker, project or user settings are involved. Not a CTest: it needs a desktop/GPU.
//   EngineEditorUiSmoke <snapshot-folder> [viewport-image.png] [--theme=id] [--stages=running,assets,...]
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
#include <thread>
#include <vector>
#include "Editor/Host/PixelatorTextures.h"
#include "Editor/Internals/Pixelator/PixelFrameSampling.h"
#include "Editor/Internals/Session/TuningWorkspace.h"
#include "Editor/Ui/Panels/Agents/AgentsDrawer.h"
#include "Terminal/TerminalSession.h"
#include "Editor/Ui/Workspace/EditorWorkspace.h"
#include "Editor/Ui/Style/EditorPalettePreset.h"
#include "Runtime/Game.h"
#include "ToolUi/ToolTheme.h"
#include "ToolUi/ToolUiContext.h"
#include "ToolUi/ToolWindow.h"

namespace {
// The Console's terminal colours (ConsoleSessions): the screen's ground is the window's.
TerminalPalette ConsoleLikePalette() {
    const auto rgb = [](const ImVec4& color) {
        const auto channel = [](float value) { return static_cast<std::uint32_t>(value * 255.0f + 0.5f) & 0xFF; };
        return (channel(color.x) << 16) | (channel(color.y) << 8) | channel(color.z);
    };
    TerminalPalette palette;
    palette.background = rgb(ToolPalette::Window);
    palette.foreground = rgb(ToolPalette::Text);
    return palette;
}
// What an agent CLI's screen could look like, written by a short PowerShell script (no agent runs).
std::string AgentScreenCommand() {
    return "powershell.exe -NoLogo -NoProfile -Command \""
           "Write-Host '> Make the Agents drawer resizable from its left edge' -ForegroundColor Gray; Write-Host ''; "
           "Write-Host '* Reading the workspace overlay first.' -ForegroundColor White; "
           "Write-Host '  Read Editor/Ui/Workspace/EditorWorkspace.cpp' -ForegroundColor DarkGray; "
           "Write-Host '  Edit Editor/Ui/Panels/Agents/AgentsDrawer.cpp (+42 -3)' -ForegroundColor DarkGray; "
           "Write-Host '  Bash python . check' -ForegroundColor DarkGray; Write-Host ''; "
           "Write-Host 'Which side should the drawer take when the Git drawer is open?' -ForegroundColor Yellow; "
           "Write-Host '  1. Beside it, on the right' -ForegroundColor White; "
           "Write-Host '  2. Over it' -ForegroundColor White\"";
}
enum class SmokeView {
    Idle,
    Running,
    Paused,
    Building,
    Inspector,
    QuickOpen,
    Palette,
    BuildFailed,
    AgentStrip,
    Agents,
    AgentsEmpty
};
// clang-format off
enum class SmokeGit { Clean, Behind, Fetching, Setup, Missing, Changes, Problem, Conflict, TextDiff, ImageDiff, Incoming };
// clang-format on
struct SmokeStage {
    const char* name;
    SmokeView view;
    int width = 1600;
    int height = 900;
    int scale = 100;
    std::optional<EditorPanel> panel = std::nullopt; // Brought to the front of its dock
    SmokeGit git = SmokeGit::Clean;
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
    SmokeStage{"tuning", SmokeView::Running, 1600, 1000, 100, EditorPanel::Inspector},
    SmokeStage{"tuning-file", SmokeView::Running, 1600, 1000, 100, EditorPanel::Inspector},
    SmokeStage{"tuning-narrow", SmokeView::Running, 1100, 680, 150, EditorPanel::Inspector},
    SmokeStage{"project-settings", SmokeView::Running, 1600, 1000, 100, EditorPanel::Inspector},
    SmokeStage{"input-map", SmokeView::Running, 1600, 1000, 100, EditorPanel::Inspector},
    SmokeStage{"notes", SmokeView::Running, 1600, 900, 100, EditorPanel::Notes},
    SmokeStage{"assets", SmokeView::Running, 1600, 900, 100, EditorPanel::Assets},
    SmokeStage{"audio", SmokeView::Running, 1600, 900, 100, EditorPanel::AudioPreview},
    SmokeStage{"pixelator", SmokeView::Running, 1600, 900, 100, EditorPanel::Pixelator},
    SmokeStage{"pixelator-narrow", SmokeView::Running, 1100, 680, 150, EditorPanel::Pixelator},
    SmokeStage{"agent-strip", SmokeView::AgentStrip},
    SmokeStage{"agents", SmokeView::Agents},
    SmokeStage{"agents-150", SmokeView::Agents, 1280, 800, 150},
    SmokeStage{"agents-empty", SmokeView::AgentsEmpty},
    SmokeStage{"quick-open", SmokeView::QuickOpen},
    SmokeStage{"palette", SmokeView::Palette},
    SmokeStage{"build-failed", SmokeView::BuildFailed},
    SmokeStage{"git-behind", SmokeView::Running, 1600, 900, 100, std::nullopt, SmokeGit::Behind},
    SmokeStage{"git-fetching", SmokeView::Running, 1600, 900, 100, std::nullopt, SmokeGit::Fetching},
    SmokeStage{"git-narrow", SmokeView::Running, 1100, 680, 100, std::nullopt, SmokeGit::Behind},
    SmokeStage{"git-narrow-150", SmokeView::Running, 1280, 760, 150, std::nullopt, SmokeGit::Behind},
    SmokeStage{"git-setup", SmokeView::Running, 1600, 900, 100, std::nullopt, SmokeGit::Setup},
    SmokeStage{"git-missing", SmokeView::Running, 1600, 900, 100, std::nullopt, SmokeGit::Missing},
    // The drawer stays open from here on: the first of these opens it.
    SmokeStage{"git-changes", SmokeView::Running, 1600, 900, 100, std::nullopt, SmokeGit::Changes},
    SmokeStage{"git-problem", SmokeView::Running, 1600, 900, 100, std::nullopt, SmokeGit::Problem},
    SmokeStage{"git-conflict", SmokeView::Running, 1600, 900, 100, std::nullopt, SmokeGit::Conflict},
    SmokeStage{"git-diff-text", SmokeView::Running, 1600, 900, 100, std::nullopt, SmokeGit::TextDiff},
    SmokeStage{"git-diff-image", SmokeView::Running, 1600, 900, 100, std::nullopt, SmokeGit::ImageDiff},
    SmokeStage{"git-incoming", SmokeView::Running, 1600, 900, 100, std::nullopt, SmokeGit::Incoming},
};
GitSnapshot SampleGit(SmokeGit view) {
    GitSnapshot git;
    git.availability = view == SmokeGit::Setup     ? GitAvailability::NoRepository
                       : view == SmokeGit::Missing ? GitAvailability::Missing
                                                   : GitAvailability::Ready;
    if (git.availability != GitAvailability::Ready) return git;
    GitRepositoryState repository;
    repository.name = "MyProject";
    repository.branch = {"main", "abc1234", "origin/main"};
    repository.remotes = {"origin"};
    repository.localBranches = {"main", "new-boss"};
    repository.remoteBranches = {"origin/main", "origin/teammate-art"};
    repository.userName = "Developer";
    repository.userEmail = "developer@example.com";
    repository.fetchedAt = std::chrono::system_clock::now() - std::chrono::minutes(2);
    GitRepositoryState submodule = repository;
    submodule.name = "Engine";
    submodule.isSubmodule = true;
    submodule.branch.ahead = 2;
    const auto now = std::chrono::system_clock::to_time_t(std::chrono::system_clock::now());
    const std::vector<GitCommitSummary> incoming{{"a1f03c2", "Update Engine", "Teammate", now - 7200},
                                                 {"77be210", "Tune card draft odds", "Teammate", now - 10800},
                                                 {"0c9d4e1", "Add boss portrait", "Artist", now - 90000}};
    const std::vector<GitChange> changes{
        {"Game/Enemies/Enemy.cpp", "", '.', 'M', GitChangeKind::Modified},
        {"Game/Enemies/Enemy.h", "", '.', 'M', GitChangeKind::Modified},
        {"Game/Enemies/Spawner.cpp", "", '?', '?', GitChangeKind::Untracked},
        {"Game/Assets/Graphics/hero.png", "", '.', 'M', GitChangeKind::Modified},
        {"Game/Assets/Audio/SE/Hit.wav", "", '.', 'M', GitChangeKind::Modified},
        {"Game/Assets/Audio/Music/", "", '?', '?', GitChangeKind::Untracked},
        {"Docs/Notes.md", "", 'D', '.', GitChangeKind::Deleted},
        {"Game/Scenes/Boss.cpp", "Game/Scenes/BossFight.cpp", 'R', '.', GitChangeKind::Renamed},
        {"Tuning/Enemies.json", "", '.', 'M', GitChangeKind::Modified}};
    git.fetching = view == SmokeGit::Fetching;
    switch (view) {
    case SmokeGit::Behind:
    case SmokeGit::Fetching:
        repository.branch.behind = 3;
        repository.incoming.commits = incoming;
        repository.incoming.submodules = {"Engine"};
        repository.changes = changes;
        break;
    case SmokeGit::Changes:
    case SmokeGit::TextDiff:
    case SmokeGit::ImageDiff: repository.changes = changes; break;
    case SmokeGit::Problem:
        repository.changes = changes;
        repository.branch.behind = 3;
        repository.incoming.commits = incoming;
        git.problem = {GitProblemKind::LocalChanges,
                       "Your local changes overlap the incoming ones.",
                       {},
                       {"Game/Enemies/Enemy.cpp", "Tuning/Enemies.json"}};
        break;
    case SmokeGit::Conflict:
        repository.sequence = GitSequence::Merge;
        repository.changes = changes;
        repository.changes.push_back({"Game/Boss/Boss.cpp", "", 'U', 'U', GitChangeKind::Conflicted});
        repository.changes.push_back({"Game/Assets/Graphics/boss.png", "", 'U', 'U', GitChangeKind::Conflicted});
        break;
    case SmokeGit::Incoming:
        repository.changes = changes;
        repository.branch.behind = 3;
        repository.incoming.commits = incoming;
        repository.incoming.submodules = {"Engine"};
        repository.incoming.overlapping = {"Tuning/Enemies.json"};
        break;
    case SmokeGit::Clean:
    case SmokeGit::Setup:
    case SmokeGit::Missing: break;
    }
    if (view == SmokeGit::TextDiff) {
        auto diff = std::make_shared<GitDiff>();
        diff->kind = GitDiffKind::Text;
        diff->path = "Game/Enemies/Enemy.cpp";
        diff->lines = {{GitDiffLineKind::Hunk, "@@ -84,7 +84,8 @@ void Enemy::Update(float delta) {"},
                       {GitDiffLineKind::Context, "     _time += delta;"},
                       {GitDiffLineKind::Removed, "-    if (_time > 2.0f) Fire();"},
                       {GitDiffLineKind::Added, "+    if (_time > FireInterval) {"},
                       {GitDiffLineKind::Added, "+        Fire();"},
                       {GitDiffLineKind::Added, "+    }"},
                       {GitDiffLineKind::Context, "     MoveTowards(_target, delta);"}};
        git.diff = std::move(diff);
    }
    if (view == SmokeGit::ImageDiff) {
        auto diff = std::make_shared<GitDiff>();
        diff->kind = GitDiffKind::Image;
        diff->path = "Game/Assets/Graphics/hero.png";
        diff->before = "before.png";
        diff->after = "after.png";
        git.diff = std::move(diff);
    }
    git.repositories = {std::move(repository), std::move(submodule)};
    return git;
}
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
    actions.tuning = [](const TuningEditRequest&) {};
    actions.inspectTuning = [](const std::filesystem::path&) {};
    actions.openSource = [](const std::string&, int) {};
    actions.previewAudio = [](const AudioPreviewRequest&) {};
    actions.openPath = [](const std::filesystem::path&) {};
    actions.assetThumbnail = [](const std::string&) { return std::uint64_t{0}; };
    actions.assetCommand = [](const std::vector<std::string>&) -> std::uint64_t { return 0; };
    actions.git = [](const GitRequest&) {};
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

// A made-up 48 x 40 pixel graphic with two items and a two-frame blink, as the worker would send it.
std::shared_ptr<const PixelatorAsset> SamplePixels() {
    const PixelPalette palette{Rgb(0x14161c), Rgb(0x3a4152), Rgb(0x66708a), Rgb(0x9aa5c0),
                               Rgb(0xe0e6f0), Rgb(0xb5601c), Rgb(0xf0a030)};
    const auto sprite = [](bool blink) {
        PixelCanvas canvas(48, 40);
        {
            const auto body = canvas.Part(1);
            canvas.Ellipse(24.0f, 24.0f, 14.0f, 13.0f, {2, 2, 3, 4, 5});
        }
        {
            const auto head = canvas.Part(2);
            canvas.Ellipse(24.0f, 10.0f, 8.0f, 7.0f, {2, 2, 3, 4, 5});
            canvas.Stamp(20, 9, {blink ? "1...1" : "6...6"});
        }
        canvas.FlatRect(30, 30, 6, 4, 7);
        canvas.Outline(1);
        return PixelSprite(canvas, {24, 40});
    };
    const auto open = sprite(false);
    const auto closed = sprite(true);
    auto asset = SamplePixelFrames(
        [&](float time) { return GraphicPixels{time - std::floor(time) < 0.8f ? open : closed, palette}; });
    asset.id = "asset:Graphics/Characters/Owl#Idle";
    asset.items = {{"Idle", "asset:Graphics/Characters/Owl#Idle"}, {"Fly", "asset:Graphics/Characters/Owl#Fly"}};
    return std::make_shared<const PixelatorAsset>(std::move(asset));
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
    [[nodiscard]] const std::filesystem::path& Root() const { return _root; }
    SmokeProject() {
        std::error_code error;
        _root = std::filesystem::temp_directory_path(error) / ("editor-ui-smoke-" + std::to_string(::time(nullptr)));
        WriteFile(EngineDirectory() / "Assets" / "Graphics" / "Icons" / "Icons.asset.cpp");
        WriteFile(EngineDirectory() / "Assets" / "Graphics" / "Backdrops" / "Stars.asset.cpp");
        for (const char* file :
             {"Graphics/Enemies/Slime.asset.cpp", "Graphics/Enemies/Dragon.asset.cpp", "Graphics/Player/Hero.asset.cpp",
              "Audio/SE/Hit.wav", "Audio/SE/Pickup.wav", "Audio/SE/Explosion.wav", "Audio/Music/Theme.ogg"})
            WriteFile(AssetsDirectory() / file);
        WriteFile(_root / "Notes/Plan.md",
                  "# Project plan\n\nKeep plans, reports and handoffs together.\n\n## Next steps\n\n- Review the "
                  "current scene.\n- Record the result for the next developer or agent.\n\nThis is a plain Markdown "
                  "note. Long paragraphs wrap to the panel width so they remain comfortable to read while working "
                  "beside the Inspector.\n");
        WriteFile(TuningFile(), EnemyTuning);
        WriteFile(_root / "Game" / "Effects" / "Glow.tuning.json",
                  "{\n  \"radius\": { \"value\": 12, \"min\": 0, \"unit\": \"px\" }\n}\n");
        WriteFile(SettingsFile(),
                  "{\n  \"title\": \"My game\",\n  \"canvasWidth\": 640,\n  \"canvasHeight\": 360\n}\n");
        WriteFile(InputMapFile(),
                  "{\n  \"buttons\": [\"Left\", \"Right\", \"Jump\", \"Pause\"],\n  \"axes\": [\"MoveX\"],\n"
                  "  \"players\": [\n    { \"Left\": [\"A\", \"Left\"], \"Right\": [\"D\", \"Right\"], "
                  "\"Jump\": [\"Space\"], \"Pause\": [\"Escape\"] },\n"
                  "    { \"Left\": [\"J\"], \"Right\": [\"L\"], \"Jump\": [\"RightControl\"] }\n  ]\n}\n");
        const auto quoted = [](const std::filesystem::path& path) { return "\"" + path.generic_string() + "\""; };
        WriteFile(_root / "build" / "Tuning" / "TuningFiles.json",
                  "{ \"version\": 2, \"project\": " + quoted(_root) + ", \"roots\": [ { \"target\": \"MyGame\", " +
                      "\"root\": " + quoted(_root / "Game") + " } ], \"files\": [ { \"target\": \"MyGame\", " +
                      "\"source\": " + quoted(TuningFile()) + " } ] }");
    }
    ~SmokeProject() {
        std::error_code error;
        std::filesystem::remove_all(_root, error);
    }
    [[nodiscard]] std::filesystem::path EngineDirectory() const { return _root / "Engine"; }
    [[nodiscard]] std::filesystem::path AssetsDirectory() const { return _root / "Game" / "Assets"; }
    [[nodiscard]] std::filesystem::path BuildDirectory() const { return _root / "build"; }
    // Read by the running scene; Effects/Glow.tuning.json is not built yet.
    [[nodiscard]] std::filesystem::path TuningFile() const { return _root / "Game" / "Enemy.tuning.json"; }
    [[nodiscard]] std::filesystem::path SettingsFile() const { return _root / "Game" / "Game.settings.json"; }
    [[nodiscard]] std::filesystem::path InputMapFile() const {
        return _root / "Game" / "Simulation" / "Pad.input.json";
    }
    static constexpr std::string_view EnemyTuning = R"({
  "radius": { "value": 12, "min": 0, "max": 40, "unit": "px", "note": "Hit circle around the enemy" },
  "speed": { "value": 2.5, "min": 0, "max": 10, "unit": "px/s" },
  "visible": { "value": true },
  "mode": { "type": "choice", "choices": ["Patrol", "Chase", "Flee"], "value": "Chase", "group": "Behaviour" },
  "tint": { "type": "color", "value": "#FF4080", "group": "Look" },
  "offset": { "type": "vec2", "value": [0, -4], "unit": "px", "group": "Look" },
  "delay": { "type": "range", "value": { "min": 0.2, "max": 0.6 }, "min": 0, "unit": "s", "group": "Behaviour" },
  "steps": { "type": "float[]", "value": [0, 0.5, 1], "group": "Behaviour" },
  "odds": {
    "type": "table",
    "group": "Balance",
    "note": "Spawn odds per level",
    "columns": {
      "fromLevel": { "type": "int", "min": 1 },
      "chances": { "type": "float[]", "min": 0, "max": 100, "unit": "%" }
    },
    "rows": [
      { "fromLevel": 1, "chances": [70, 30] },
      { "fromLevel": 5, "chances": [40, 60] }
    ]
  }
}
)";

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
    auto tint = Parameter("tint", "Arena tint", SceneParameterKind::Color, Color4{77, 128, 179, 255}, "Debug");
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
    EditorUiSmoke(std::filesystem::path output, const std::filesystem::path& image, const EditorPalettePreset& palette,
                  std::vector<SmokeStage> stages)
        : Game(SmokeSettings())
        , _output(std::move(output))
        , _stages(std::move(stages))
        , _palette(palette)
        , _catalog(SampleCatalog())
        , _parameters(SceneDefaults(_catalog.front()))
        , _ui(ToolUiOptions{.docking = true}) {
        _preferences.theme = palette.id;
        _ui.SetPalette(palette.colors);
        ApplyToolPalette(_expectedColors, palette.colors);
        _parameters["level"] = std::int64_t{7};
        _status = "Scene running";
        FailBuild();
        _diagnostics.Acknowledge();
        if (!image.empty()) _frame = LoadTexture(image.string().c_str());
        _preferences.Visible(EditorPanel::Console) = true;
        _preferences.recent = {"sample.fight", "sample.Boss intro", "sample.category.Visuals"};
        _workspace.SetAssetRoots(_project.EngineDirectory(), _project.AssetsDirectory());
        _workspace.SetProjectName("MyProject");
        _workspace.SetNotesProject(_project.Root());
        _tuning.Open(_project.BuildDirectory());
        // The scene reads Enemy.tuning.json live; its commit had another radius and speed, so both show Modified.
        _tuning.SetUsed({_project.TuningFile()});
        (void)_tuning.Report({_project.TuningFile().generic_string(), WorkerTuningState::Applied});
        auto committed = std::string(SmokeProject::EnemyTuning);
        for (const auto& [now, then] :
             {std::pair{"\"value\": 12", "\"value\": 10"}, {"\"value\": 2.5", "\"value\": 3.5"}})
            committed.replace(committed.find(now), std::string_view(now).size(), then);
        _tuning.SetCommitted(_project.TuningFile(), committed);
        const auto picture = [](Color first, Color second) {
            const Image image = GenImageChecked(64, 64, 8, 8, first, second);
            const Texture2D texture = LoadTextureFromImage(image);
            UnloadImage(image);
            return texture;
        };
        _before = picture({95, 109, 87, 255}, {58, 69, 55, 255});
        _after = picture({244, 175, 101, 255}, {92, 63, 34, 255});
        _actions.pixelatorTexture = [this](std::size_t slot, int width, int height, std::span<const Color4> pixels) {
            return _pixelatorTextures.Upload(slot, width, height, pixels);
        };
        _actions.gitImage = [this](const std::filesystem::path& file) {
            const auto& texture = file == "before.png" ? _before : _after;
            return EditorGitImage{texture.id, texture.width, texture.height};
        };
        const auto steadyNow = std::chrono::steady_clock::now();
        _agentInfos = {{.id = 1,
                        .title = "Codex",
                        .agent = AgentKind::Codex,
                        .state = SessionState::Working,
                        .activity = "Building the Agents panel",
                        .started = steadyNow - std::chrono::minutes(18),
                        .lastOutput = steadyNow - std::chrono::seconds(8)},
                       {.id = 2,
                        .title = "Claude 2",
                        .agent = AgentKind::ClaudeCode,
                        .state = SessionState::Waiting,
                        .activity = "Which layout should I update?",
                        .started = steadyNow - std::chrono::minutes(11),
                        .lastOutput = steadyNow - std::chrono::minutes(1)},
                       {.id = 3,
                        .title = "Codex 2",
                        .agent = AgentKind::Codex,
                        .state = SessionState::Done,
                        .activity = "Editor tests passed",
                        .started = steadyNow - std::chrono::minutes(24),
                        .lastOutput = steadyNow - std::chrono::minutes(4)}};
        const auto systemNow = std::chrono::system_clock::now();
        auto& working = _agentReport.sessions["1"];
        working.note = AgentNote{AgentActivityState::Working, "Building the Agents panel", "1"};
        working.noteTime = steadyNow - std::chrono::seconds(8);
        working.tools = {{systemNow - std::chrono::minutes(2), "Read EditorWorkspace.cpp"},
                         {systemNow - std::chrono::seconds(15), "Edit AgentsDrawer.cpp"}};
        working.toolCount = 7;
        auto& waiting = _agentReport.sessions["2"];
        waiting.note = AgentNote{AgentActivityState::Waiting, "Which layout should I update?", "2"};
        waiting.noteTime = steadyNow - std::chrono::minutes(1);
        waiting.tools = {{systemNow - std::chrono::minutes(3), "Read Docs/Design/README.md"}};
        waiting.toolCount = 3;
        auto& done = _agentReport.sessions["3"];
        done.note = AgentNote{AgentActivityState::Idle, "Editor tests passed", "3"};
        done.noteTime = steadyNow - std::chrono::minutes(4);
        done.tools = {{systemNow - std::chrono::minutes(5), "Run EngineEditorTests"}};
        done.toolCount = 4;
        _agentReport.hold = AgentEditorHold{"1", 12.0};
        // A real terminal showing what an agent CLI's screen looks like (made-up text; no agent runs)
        _agentTerminal.Start({.commandLine = AgentScreenCommand()});
        _agentReport.visualChecks = false;
        for (std::size_t index = 0; index < _events.size(); ++index)
            _events[index].session = index < 2 ? "1" : index == 2 ? "2" : "3";
    }
    ~EditorUiSmoke() override {
        if (_frame.id != 0) UnloadTexture(_frame);
        UnloadTexture(_before);
        UnloadTexture(_after);
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
        if (_stage == _stages.size()) {
            RequestQuit();
            return;
        }
        if (_frameIndex == 0) SetWindowSize({_stages[_stage].width, _stages[_stage].height});
        _agentTerminal.Pump();
        // The drawer's terminal shows its text once the program has written and ended (a few frames are not enough)
        if (_stages[_stage].view == SmokeView::Agents && _frameIndex == FramesPerStage - 1 &&
            _agentTerminal.Running() && _agentWaits++ < 200) {
            std::this_thread::sleep_for(std::chrono::milliseconds(25));
            return;
        }
        ++_frameIndex;
        _tuning.Poll();
    }
    void DrawToolOverlay() const override {
        if (_stage == _stages.size()) return;
        const auto& stage = _stages[_stage];
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
        if (std::string_view(stage.name) == "tuning-file") snapshot.inspectedTuning = _project.TuningFile();
        if (std::string_view(stage.name) == "project-settings") snapshot.inspectedTuning = _project.SettingsFile();
        if (std::string_view(stage.name) == "input-map") snapshot.inspectedTuning = _project.InputMapFile();
        snapshot.audio.file = _project.AssetsDirectory() / "Audio" / "SE" / "Hit.wav";
        snapshot.audio.length = _waveform->seconds;
        snapshot.audio.position = 0.7f;
        snapshot.audio.waveform = {WaveformState::Ready, _waveform, {}};
        snapshot.pixels = _pixels;
        snapshot.controlEvents = &_events;
        snapshot.controlEventCount = _events.size();
        if (stage.view == SmokeView::Agents || stage.view == SmokeView::AgentStrip) snapshot.agents = &_agentReport;
        if (stage.view == SmokeView::AgentsEmpty) snapshot.agents = &_emptyReport;
        // A failure reported through the control endpoint opens no popup; only the one stage wants it.
        snapshot.controlAttempt = stage.view == SmokeView::BuildFailed ? 0 : _diagnostics.Attempt().id;
        const ViewportImage image{_frame.id, _frame.width, _frame.height, 1, 1, 0.0};
        const auto git = SampleGit(stage.git);
        snapshot.git = &git;
        // Escape closes what the last stage opened, then the keys that open this stage's palette.
        auto& io = ImGui::GetIO();
        const bool gitDrawerStage = stage.git == SmokeGit::Changes || stage.git == SmokeGit::Problem ||
                                    stage.git == SmokeGit::Conflict || stage.git == SmokeGit::TextDiff ||
                                    stage.git == SmokeGit::ImageDiff || stage.git == SmokeGit::Incoming;
        if (gitDrawerStage && _frameIndex == 3) {
            const auto* drawer = ImGui::FindWindowByName("Source control###editor.git-drawer");
            _openingGit = !drawer || !drawer->WasActive;
        }
        if (gitDrawerStage && _openingGit && _frameIndex >= 3 && _frameIndex <= 4) {
            io.AddKeyEvent(ImGuiMod_Ctrl, _frameIndex == 3);
            io.AddKeyEvent(ImGuiMod_Shift, _frameIndex == 3);
            io.AddKeyEvent(ImGuiKey_G, _frameIndex == 3);
        }
        if (stage.git == SmokeGit::Incoming && _frameIndex >= 4 && _frameIndex <= 6) {
            // The drawer's second tab, by where it sits.
            if (const auto* drawer = ImGui::FindWindowByName("Source control###editor.git-drawer")) {
                constexpr ImVec2 TabOffset{130, 24};
                io.AddMousePosEvent(drawer->Pos.x + TabOffset.x, drawer->Pos.y + TabOffset.y);
                if (_frameIndex > 4) io.AddMouseButtonEvent(ImGuiMouseButton_Left, _frameIndex == 5);
            }
        }
        // A filtered run can jump straight from Quick open to a non-popup stage.
        const bool floating = stage.view == SmokeView::QuickOpen || stage.view == SmokeView::Palette ||
                              stage.view == SmokeView::BuildFailed;
        const bool popup = ImGui::IsPopupOpen(nullptr, ImGuiPopupFlags_AnyPopupId | ImGuiPopupFlags_AnyPopupLevel);
        if (_frameIndex <= 2) io.AddKeyEvent(ImGuiKey_Escape, _frameIndex == 1 && (floating || popup));
        if ((stage.view == SmokeView::QuickOpen || stage.view == SmokeView::Palette) && _frameIndex >= 3 &&
            _frameIndex <= 4) {
            io.AddKeyEvent(ImGuiMod_Ctrl, _frameIndex == 3);
            if (stage.view == SmokeView::Palette) io.AddKeyEvent(ImGuiMod_Shift, _frameIndex == 3);
            io.AddKeyEvent(ImGuiKey_P, _frameIndex == 3);
        }
        _ui.BeginFrame(stage.scale);
        // Real ToolUiContext coverage: a later scale change must not restore Cabinet's baseline colors.
        const auto& style = ImGui::GetStyle();
        if (!std::equal(_expectedColors.begin(), _expectedColors.end(), std::begin(style.Colors),
                        [](ImVec4 a, ImVec4 b) { return a.x == b.x && a.y == b.y && a.z == b.z && a.w == b.w; })) {
            std::cerr << stage.name << ": palette lost after BeginFrame (" << _palette.key << ")\n";
            ++_failures;
        }
        if (stage.panel) {
            _preferences.Visible(*stage.panel) = true;
            SelectTab(EditorPanelInfo(*stage.panel).window);
        }
        if (std::string_view(stage.name) == "notes") {
            _preferences.Visible(EditorPanel::Console) = false;
            _preferences.Visible(EditorPanel::Assets) = true;
            SelectTab(EditorPanelInfo(EditorPanel::Assets).window);
            if (_frameIndex == 1) _workspace.OpenNote(_project.Root() / "Notes/Plan.md", _preferences);
        }
        (void)_workspace.Draw(snapshot, _actions, _frame.id != 0 ? image : ViewportImage{}, _preferences, _fontSize);
        if (stage.view == SmokeView::AgentStrip) DrawAgentStrips(snapshot);
        if (stage.view == SmokeView::Agents || stage.view == SmokeView::AgentsEmpty) {
            // Where the workspace puts it: below the toolbar, above the status bar, on the right
            const auto* toolbar = ImGui::FindWindowByName("Toolbar###editor.toolbar");
            const auto* bar = ImGui::FindWindowByName("Status###editor.status");
            const float top = toolbar ? toolbar->Pos.y + toolbar->Size.y : 0.0f;
            const float bottom = bar ? bar->Pos.y : ImGui::GetIO().DisplaySize.y;
            _agentsDrawer.Open(2);
            const bool empty = stage.view == SmokeView::AgentsEmpty;
            (void)_agentsDrawer.Draw(empty ? std::vector<ConsoleSessionInfo>{} : _agentInfos,
                                     [this](std::uint64_t id) { return id == 2 ? &_agentTerminal : nullptr; },
                                     _agentViews, snapshot, _actions, {0, top},
                                     {ImGui::GetIO().DisplaySize.x, bottom - top});
        }
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
        // Several agents: one waits, one works and holds the editor, one is done
        EditorAgentStatus several{.session = true,
                                  .title = "Codex 2",
                                  .state = SessionState::Waiting,
                                  .activity = "Which layout should I update?"};
        several.sessions = {{SessionState::Working, true}, {SessionState::Waiting}, {SessionState::Done},
                            {SessionState::Idle},          {SessionState::Working}, {SessionState::Done}};
        for (const float width : Widths) {
            ImGui::PushID(static_cast<int>(strip));
            _agentStrips[strip++].Draw(snapshot, _actions, several, width);
            ImGui::PopID();
            ImGui::SameLine();
        }
        ImGui::NewLine();
        ImGui::End();
    }

    std::filesystem::path _output;
    std::vector<SmokeStage> _stages;
    const EditorPalettePreset& _palette;
    std::array<ImVec4, ImGuiCol_COUNT> _expectedColors;
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
    std::shared_ptr<const PixelatorAsset> _pixels = SamplePixels();
    mutable PixelatorTextures _pixelatorTextures;
    std::deque<EditorControlEvent> _events = SampleEvents();
    Texture2D _frame{};
    Texture2D _before{};
    Texture2D _after{};
    mutable EditorPreferences _preferences;
    mutable EditorWorkspace _workspace;
    mutable std::array<EditorAgentActivity, 15> _agentStrips;
    std::vector<ConsoleSessionInfo> _agentInfos;
    EditorAgentReport _agentReport;
    EditorAgentReport _emptyReport; // No agent has run yet
    mutable AgentsDrawer _agentsDrawer;
    mutable ConsoleViews _agentViews;
    mutable TerminalSession _agentTerminal{ConsoleLikePalette()};
    int _agentWaits = 0;
    mutable int _fontSize = 100;
    mutable int _failures = 0;
    mutable bool _openingGit = false;
    mutable ToolUiContext _ui;
    std::size_t _stage = 0;
    int _frameIndex = 0;
};
} // namespace

int main(int argc, char** argv) {
    if (argc < 2) {
        std::cerr << "Usage: EngineEditorUiSmoke <snapshot-folder> [viewport-image.png] [--theme=id] [--stages=a,b]\n";
        return 2;
    }
    const auto output = std::filesystem::absolute(argv[1]);
    std::filesystem::create_directories(output);
    std::filesystem::path image;
    EditorTheme theme = EditorTheme::Cabinet;
    std::string filter;
    for (int i = 2; i < argc; ++i) {
        const std::string_view argument(argv[i]);
        if (argument.starts_with("--theme="))
            theme = ParseEditorTheme(argument.substr(8));
        else if (argument.starts_with("--stages="))
            filter = "," + std::string(argument.substr(9)) + ",";
        else
            image = argv[i];
    }
    std::vector<SmokeStage> stages;
    for (const auto& stage : Stages)
        if (filter.empty() || filter.find("," + std::string(stage.name) + ",") != std::string::npos)
            stages.push_back(stage);
    if (stages.empty()) {
        std::cerr << "No matching smoke stages\n";
        return 2;
    }
    EditorUiSmoke smoke(output, image, ResolveEditorPalette(theme), std::move(stages));
    smoke.Run();
    std::cout << "UI checks: " << smoke.Failures() << " failures\n";
    return smoke.Failures() == 0 ? 0 : 1;
}

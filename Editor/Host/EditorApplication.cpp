#include "Editor/Host/EditorApplication.h"
#include <imgui.h>
#include <nlohmann/json.hpp>
#include <algorithm>
#include <chrono>
#include <filesystem>
#include <format>
#include <fstream>
#include <map>
#include <stdexcept>
#include <string_view>
#include <utility>
#include <vector>
#include "Editor/Host/EditorEntryPoint.h"
#include "Editor/Host/FramePng.h"
#include "Editor/Host/ProjectListLauncher.h"
#include "Editor/Internals/Session/AgentContext.h"
#include "Editor/Internals/Session/ViewportMark.h"
#include "Editor/Ui/Style/EditorPalettePreset.h"
#include "Input/Internals/InputReader.h"
#include "Platform/Internals/UserData.h"
#include "Platform/Shell.h"
#include "ToolUi/ToolTheme.h"
#include "ToolUi/ToolWindow.h"

namespace {
constexpr double SettingsSaveDelay = 0.5;
constexpr double LastFrameSeconds = 30.0; // Rare enough that encoding it never shows in the scene's frame rate
constexpr double LastFrameMaxSide = 960.0;
// Beside the target and renamed over it, so a reader never sees half a file.
bool WriteStatusFile(const std::filesystem::path& file, std::string_view content) {
    std::error_code error;
    std::filesystem::create_directories(file.parent_path(), error);
    auto partial = file;
    partial += ".partial";
    {
        std::ofstream stream(partial, std::ios::binary);
        stream.write(content.data(), static_cast<std::streamsize>(content.size()));
        if (!stream) return false;
    }
    std::filesystem::rename(partial, file, error);
    return !error;
}
GameSettings EditorSettings() {
    WindowSettings window;
    window.title = "Engine Editor";
    window.width = 1440;
    window.height = 900;
    window.minWidth = 1000;
    window.minHeight = 650;
    window.targetFps = 60;
    window.alwaysRun = true;
    window.maximized = true;
    return ToolGameSettings("EngineEditor", window);
}
} // namespace
int RunEngineEditor(const std::filesystem::path& projectFile) {
    EditorApplication application(projectFile);
    application.Run();
    return 0;
}
EditorApplication::EditorApplication(const std::filesystem::path& projectFile)
    : Game(EditorSettings())
    , _controller(projectFile)
    , _ui(ToolUiOptions{.docking = true}) {
    try {
        if (const auto& project = _controller.Project()) {
            _control.emplace(project->buildDirectory,
                             EditorCaptureHost{EncodeFramePng, [this] { return RequestWindow(); },
                                               [this](std::uint64_t sequence) { return Window(sequence); }});
            _workspace.SetConsoleEnvironment({{"ENGINE_EDITOR_CONTROL", _control->Pipe()}});
            _control->SetAudioHost({[this] { return _audio; },
                                    [this](const AudioPreviewRequest& request) {
                                        // An agent's sound comes into view, as Assets' would for a click
                                        if (request.action == AudioPreviewAction::Play)
                                            _preferences.Visible(EditorPanel::AudioPreview) = true;
                                        return QueueAudio(request);
                                    }});
        }
    } catch (const std::exception& error) {
        _controller.Log(std::string("Control endpoint unavailable: ") + error.what(), EditorLogLevel::Warning);
    }
    try {
        _projectKey = _controller.ProjectKey();
        _settings.emplace(UserDataDirectory() / "EngineEditor" / "preferences.json");
        if (auto error = _settings->Load(_projectKey, _preferences, _fontSize); !error.empty())
            _controller.Log(error, EditorLogLevel::Warning);
    } catch (const std::exception& error) {
        _controller.Log(error.what(), EditorLogLevel::Warning);
    }
    _observedPreferences = _preferences;
    _observedFontSize = _fontSize;
    _launcher = LauncherFromEnvironment();
    _workspace.OfferProjectList(!_launcher.empty());
    if (const auto& project = _controller.Project()) {
        _workspace.SetProjectName(project->name);
        _workspace.SetConsoleDirectory(project->sourceDirectory);
        _workspace.SetAssetRoots(project->engineDirectory, project->assetsDirectory);
        const auto background = ImGui::ColorConvertFloat4ToU32(ToolPalette::Content);
        _assetCommands.Configure(project->python, project->engineDirectory, project->sourceDirectory);
        if (!project->sourceDirectory.empty())
            _git = std::make_unique<GitService>(
                GitServiceOptions{project->sourceDirectory, project->buildDirectory / "git",
                                  project->engineDirectory / "Tools" / "ProjectTemplate", project->name});
        _thumbnails.Configure(
            project->worker, project->buildDirectory / "asset-thumbnails",
            std::format("{:02x}{:02x}{:02x}", background & 0xFF, (background >> 8) & 0xFF, (background >> 16) & 0xFF));
    }
}
EditorApplication::~EditorApplication() { Stop(); }
DeviceInput EditorApplication::ReadInput() {
    _windowInput = ReadInputDevices();
    return {};
}
void EditorApplication::SavePreferences(bool force) {
    if (!_settings) return;
    if (_preferences != _observedPreferences || _fontSize != _observedFontSize) {
        _observedPreferences = _preferences;
        _observedFontSize = _fontSize;
        _saveAt = WorkerTimestamp() + SettingsSaveDelay;
        _dirty = true;
    }
    if (!_dirty || (!force && WorkerTimestamp() < _saveAt)) return;
    if (auto error = _settings->Save(_projectKey, _preferences, _fontSize); !error.empty())
        _controller.Log(error, EditorLogLevel::Warning);
    _dirty = false;
}
void EditorApplication::UpdateProjectStatus() {
    const auto& project = _controller.Project();
    if (!project) return;
    const auto snapshot = _controller.Snapshot();
    const auto& attempt = snapshot.diagnostics.Attempt();
    const bool finished = attempt.result == EditorBuildResult::Succeeded ||
                          attempt.result == EditorBuildResult::BuildFailed ||
                          attempt.result == EditorBuildResult::RefreshFailed;
    if (finished && attempt.id != _statusAttempt) {
        _statusAttempt = attempt.id;
        const auto now = std::chrono::floor<std::chrono::seconds>(std::chrono::system_clock::now());
        const nlohmann::json status{{"ok", attempt.result == EditorBuildResult::Succeeded},
                                    {"finished", std::format("{:%FT%T}Z", now)}};
        (void)WriteStatusFile(project->buildDirectory / "LastBuild.json", status.dump() + "\n");
    }
    if (snapshot.ready && WorkerTimestamp() - _lastFrameAt >= LastFrameSeconds) SaveLastFrame();
}
void EditorApplication::SaveLastFrame() {
    const auto& project = _controller.Project();
    const auto frame = _controller.LatestFrame();
    if (!project || !frame) return;
    _lastFrameAt = WorkerTimestamp();
    try {
        const auto snapshot = _controller.Snapshot();
        const double scale =
            std::min(1.0, LastFrameMaxSide / std::max(snapshot.canvas.x, std::max(snapshot.canvas.y, 1)));
        const auto png = EncodeFramePng(CaptureFrame(*frame, snapshot.canvas, {std::nullopt, scale}), {});
        (void)WriteStatusFile(project->buildDirectory / "LastFrame.png",
                              std::string_view(reinterpret_cast<const char*>(png.data()), png.size()));
    } catch (const std::exception&) { // Only informs a launcher; never in the way of working
    }
}
void EditorApplication::Update(float realSeconds) {
    (void)realSeconds;
    if (_quit) RequestQuit();
    _controller.Tick();
    if (_control) _control->Tick(_controller);
    UpdateProjectStatus();
    if (_controller.Snapshot().profiler.busy && !_beforeBenchmark) {
        _beforeBenchmark = FrameExecution();
        auto execution = *_beforeBenchmark;
        execution.fpsLimit = 10;
        execution.vsync = false;
        ConfigureFrameExecution(execution);
    } else if (!_controller.Snapshot().profiler.busy && _beforeBenchmark) {
        ConfigureFrameExecution(*_beforeBenchmark);
        _beforeBenchmark.reset();
    }
    if (auto opened = _controller.TakeOpenedScene()) _preferences.Opened(*opened);
    if (_windowMark) { // Captured at the end of a frame drawn without the marking layer
        if (!_windowMarkSequence)
            _windowMarkSequence = RequestWindow();
        else if (const auto window = Window(_windowMarkSequence)) {
            PasteWindowMark(*_windowMark, *window);
            _windowMark.reset();
            _windowMarkSequence = 0;
        }
    }
    if (_mark) {
        PasteMark(*_mark);
        _mark.reset();
    }
    if (_sendScene) {
        SendScene(_sendSceneSession);
        _sendScene = false;
        _sendSceneSession.reset();
    }
    if (!_agentProblem.empty()) _controller.Log(std::exchange(_agentProblem, {}), EditorLogLevel::Warning);
    for (const auto& request : std::exchange(_audioRequests, {}))
        HandleAudioPreview(request);
    _audio.playing = Audio().IsMusicPlaying();
    _audio.position = Audio().MusicPosition();
    if (!_audio.playing && !_audio.paused) _audio.position = 0; // Ended or stopped
    if (!_audio.file.empty()) _audio.waveform = _waveforms.Request(_audio.file);
    const auto snapshot = _controller.Snapshot();
    _assetCommands.Tick([this](const std::string& text, bool failed) {
        _controller.Log(text, failed ? EditorLogLevel::Warning : EditorLogLevel::Info);
    });
    _thumbnails.Tick(
        snapshot.building || snapshot.phase == EditorPhase::Building,
        std::ranges::any_of(snapshot.catalog, [](const SceneDefinition& entry) { return entry.asset.has_value(); }),
        [this](const std::string& problem) { _controller.Log(problem, EditorLogLevel::Warning); });
    if (_git) {
        _git->SetFocused(IsWindowFocused());
        _git->SetBuildActive(snapshot.building || snapshot.phase == EditorPhase::Building);
        for (const auto& line : _git->TakeLog())
            _controller.Log(line);
        _gitSnapshot = _git->Snapshot(); // Before asking whether it is busy: a new count implies it no longer is
        _controller.SetSourcesBusy(_git->SourcesBusy());
        // What a pull brought should run: the next Run shows the new version, and a broken one fails now.
        if (_gitSnapshot->sourcesChanged != _gitSourcesChanged) {
            _gitSourcesChanged = _gitSnapshot->sourcesChanged;
            _controller.Execute(EditorCommand::Refresh);
        }
    }
    if (_control) {
        _control->SetAgentControl(_preferences.agent.control);
        _control->SetVisualChecks(_preferences.agent.visualChecks);
        // Sessions whose program still runs; a holder whose program ended gives up the editor.
        std::map<std::string, std::string> sessions;
        for (const auto& info : _workspace.Sessions().Infos())
            if (info.state != SessionState::Exited) sessions.emplace(std::to_string(info.id), info.title);
        _control->SetConsoleSessions(std::move(sessions));
        if (std::exchange(_developerUsedEditor, false)) _control->DeveloperUsedEditor();
    }
    SavePreferences(false);
}
std::uint64_t EditorApplication::RequestWindow() const {
    _windowWanted = true;
    return _windowSequence + 1;
}
std::shared_ptr<const ViewportPixels> EditorApplication::Window(std::uint64_t sequence) const {
    return _window && _window->sequence >= sequence ? _window : nullptr;
}
void EditorApplication::PasteWindowMark(const FloatRect& region, const ViewportPixels& window) {
    try {
        if (!_control) throw std::runtime_error("Capture is unavailable without an editor control session.");
        const Vec2 size{window.width, window.height};
        const double scale =
            std::min(1.0, static_cast<double>(MarkMaxImageSide / std::max(region.width, region.height)));
        const auto image = CaptureFrame(window, size, {region, scale});
        const auto file = _control->WriteCapture(image, {}, {}, "mark");
        const auto snapshot = _controller.Snapshot();
        _workspace.PasteToConsole(
            WindowMarkContext(region, size, file, snapshot.selected ? snapshot.selected->id : std::string()),
            _preferences, "Window mark");
    } catch (const std::exception& error) {
        const auto problem = std::string("Window mark capture failed: ") + error.what();
        _controller.Log(problem, EditorLogLevel::Warning);
        _workspace.NotifyContext(problem, true);
    }
}
void EditorApplication::PasteMark(const FloatRect& region) {
    try {
        const auto snapshot = _controller.Snapshot();
        const auto frame = _controller.LatestFrame();
        if (!_control || !frame || !snapshot.selected)
            throw std::runtime_error("The scene frame is no longer available for capture.");
        const double scale =
            std::min(1.0, static_cast<double>(MarkMaxImageSide / std::max(region.width, region.height)));
        const auto image = CaptureFrame(*frame, snapshot.canvas, {region, scale});
        const auto file = _control->WriteCapture(image, {}, {}, "mark");
        _workspace.PasteToConsole(MarkContext(*snapshot.selected, snapshot.parameters, _controller.Seconds(),
                                              snapshot.paused, region, snapshot.canvas, file),
                                  _preferences, "Scene mark");
    } catch (const std::exception& error) {
        const auto problem = std::string("Scene mark capture failed: ") + error.what();
        _controller.Log(problem, EditorLogLevel::Warning);
        _workspace.NotifyContext(problem, true);
    }
}
void EditorApplication::SendScene(std::optional<std::uint64_t> session) {
    try {
        const auto snapshot = _controller.Snapshot();
        const auto frame = _controller.LatestFrame();
        if (!_control || !frame || !snapshot.selected)
            throw std::runtime_error("The scene frame is no longer available for capture.");
        const double scale = std::min(1.0, static_cast<double>(MarkMaxImageSide) /
                                               std::max(snapshot.canvas.x, std::max(snapshot.canvas.y, 1)));
        const auto image = CaptureFrame(*frame, snapshot.canvas, {std::nullopt, scale});
        const auto file = _control->WriteCapture(image, {}, {}, "scene");
        const auto text = SceneContext(*snapshot.selected, snapshot.parameters, _controller.Seconds(), snapshot.paused,
                                       snapshot.canvas, file);
        const auto problem = session ? _workspace.SendToSession(*session, text, false, "Scene context")
                                     : _workspace.SendToAgent(text, false, _preferences, "Scene context");
        if (!problem.empty()) _controller.Log(problem, EditorLogLevel::Warning);
    } catch (const std::exception& error) {
        const auto problem = std::string("Scene context capture failed: ") + error.what();
        _controller.Log(problem, EditorLogLevel::Warning);
        _workspace.NotifyContext(problem, true);
    }
}
void EditorApplication::HandleAudioPreview(const AudioPreviewRequest& request) {
    constexpr MusicId PreviewId{};
    auto& audio = Audio();
    try {
        switch (request.action) {
        case AudioPreviewAction::Play:
            _audio = {request.file, false, false, request.loop, request.volume, 0, 0, {}, {}, _audio.handled};
            audio.SetMusicVolume(request.volume);
            // From the source file, so a sound added since the last build plays without Build & Refresh.
            LoadMusicFile(audio, PreviewId, request.file);
            audio.PlayMusic(PreviewId, {.loop = request.loop});
            _audio.length = audio.MusicLength();
            break;
        case AudioPreviewAction::Pause:
            audio.PauseMusic();
            _audio.paused = audio.CurrentMusic().has_value();
            break;
        case AudioPreviewAction::Resume:
            if (_audio.paused) audio.ResumeMusic();
            _audio.paused = false;
            break;
        case AudioPreviewAction::Stop:
            audio.StopMusic();
            _audio.paused = false;
            break;
        case AudioPreviewAction::Seek: audio.SeekMusic(request.seconds); break;
        case AudioPreviewAction::Volume:
            _audio.volume = request.volume;
            audio.SetMusicVolume(request.volume);
            break;
        case AudioPreviewAction::Loop:
            _audio.loop = request.loop;
            audio.SetMusicLooping(request.loop);
            break;
        }
    } catch (const std::exception& error) {
        _audio.error = error.what();
        _controller.Log(std::string("Sound preview: ") + error.what(), EditorLogLevel::Warning);
    }
    ++_audio.handled;
}
std::uint64_t EditorApplication::QueueAudio(const AudioPreviewRequest& request) const {
    _audioRequests.push_back(request);
    return ++_audioQueued;
}
void EditorApplication::Stop() {
    if (_stopped) return;
    _workspace.CaptureLayout(_preferences);
    SavePreferences(true);
    SaveLastFrame();
    _git.reset();
    _control.reset();
    _controller.Shutdown();
    _stopped = true;
}
void EditorApplication::DrawToolOverlay() const {
    if (auto frame = _controller.TakeFrame()) {
        const bool timed = _controller.Snapshot().profiler.liveEnabled;
        const auto transferStarted = timed ? std::chrono::steady_clock::now() : std::chrono::steady_clock::time_point{};
        if (!_display.Upload(frame->width, frame->height, frame->rgba, _controller.Generation(), frame->sequence,
                             (WorkerTimestamp() - frame->capturedAt) * 1000))
            _controller.Log("Could not create the scene texture.", EditorLogLevel::Error);
        if (timed)
            _controller.HostTransfer(
                std::chrono::duration<double, std::milli>(std::chrono::steady_clock::now() - transferStarted).count());
    }
    if (_appliedTheme != _preferences.theme) {
        const auto& palette = ResolveEditorPalette(_preferences.theme);
        _ui.SetPalette(palette.colors);
        _appliedTheme = palette.id;
    }
    _ui.BeginFrame(_fontSize);
    // Defer mutations until all panels have consumed the same borrowed snapshot.
    std::vector<std::function<void()>> commands;
    bool editorInput = false;
    // What the developer does to the scene, its parameters, playback or sounds ends an agent's hold on the editor.
    const auto developer = [this] { _developerUsedEditor = true; };
    const EditorUiActions actions{
        [&](EditorCommand command) {
            editorInput = true;
            developer();
            commands.emplace_back([this, command] { _controller.Execute(command); });
        },
        [&](const std::string& id) {
            editorInput = true;
            developer();
            commands.emplace_back([this, id] { _controller.Select(id); });
        },
        [&](const std::string& id, const SceneValue& value) {
            developer();
            commands.emplace_back([this, id, value] { _controller.Parameter(id, value); });
        },
        [&](const std::string& id) {
            editorInput = true;
            developer();
            commands.emplace_back([this, id] { _controller.Action(id); });
        },
        [&] { commands.emplace_back([this] { _controller.ClearOutput(); }); },
        [&] { commands.emplace_back([this] { _controller.AcknowledgeOutput(); }); },
        [&](double speed, bool step) {
            editorInput = true;
            developer();
            commands.emplace_back([this, speed, step] { _controller.Playback(speed, step); });
        },
        [&](EditorProfilerCommand command, Profiler::Recipe recipe, const std::string& file) {
            editorInput = true;
            developer();
            commands.emplace_back([this, command, recipe, file] { _controller.Profile(command, recipe, file); });
        },
        [&](bool attempt) { commands.emplace_back([this, attempt] { _controller.OpenOutputLog(attempt); }); },
        [&](const TuningEditRequest& request) {
            developer();
            commands.emplace_back([this, request] { _controller.Tuning(request); });
        },
        [&](const std::filesystem::path& file) {
            commands.emplace_back([this, file] { _controller.InspectTuning(file); });
        },
        [&](const AudioPreviewRequest& request) {
            developer();
            (void)QueueAudio(request);
        },
        [&](const std::filesystem::path& path) {
            const auto text = path.u8string();
            const std::string_view utf8(reinterpret_cast<const char*>(text.data()), text.size());
            if (utf8.starts_with("http://") || utf8.starts_with("https://"))
                OpenExternalUrl(utf8);
            else
                commands.emplace_back([this, path] { _controller.OpenPath(path); });
        },
        [&](const std::string& id) { return _thumbnails.Texture(id); },
        [&](const std::vector<std::string>& arguments) { return _assetCommands.Run(arguments); },
        [&](std::uint64_t id) { return _assetCommands.Result(id); },
        [&](const std::string& file, int line) {
            commands.emplace_back([this, file, line] { _controller.OpenSource(file, line); });
        },
        [&](const GitRequest& request) {
            if (_git) _git->Request(request);
        },
        [&](const std::filesystem::path& file) { return _gitImages.Load(file); }};
    auto snapshot = _controller.Snapshot();
    snapshot.audio = _audio;
    snapshot.git = _gitSnapshot.get();
    if (_control) snapshot.agents = &_control->Report();
    const auto result = _workspace.Draw(snapshot, actions, _display.Image(), _preferences, _fontSize);
    if (result.interaction.pauseForMarking) {
        editorInput = true;
        developer();
        commands.emplace_back([this] { _controller.PauseForMarking(); });
    }
    _quit = result.quit;
    if (result.interaction.mark) _mark = result.interaction.mark;
    if (result.windowMark) _windowMark = result.windowMark;
    if (result.sendScene) {
        _sendScene = true;
        _sendSceneSession = result.sendSceneSession;
    }
    if (!result.agentProblem.empty()) _agentProblem = result.agentProblem;
    if (result.returnToProjectList) {
        if (auto error = StartLauncher(_launcher); error.empty())
            _quit = true;
        else
            _controller.Log(error, EditorLogLevel::Error);
    }
    const auto& io = ImGui::GetIO();
    const bool popup = ImGui::IsPopupOpen(nullptr, ImGuiPopupFlags_AnyPopupId | ImGuiPopupFlags_AnyPopupLevel);
    const bool keyboardBlocked =
        popup || io.WantTextInput || (ImGui::IsAnyItemActive() && !result.interaction.contentActive);
    const bool pointerBlocked =
        popup || !result.interaction.hovered || result.interaction.marking ||
        (ImGui::IsAnyItemActive() && !result.interaction.contentActive && !_router.OwnsPointer());
    if (editorInput || result.cancelViewportInput) _workspace.ReleaseInput();
    if (editorInput || result.cancelViewportInput || result.interaction.viewport.size.x <= 0 ||
        result.interaction.viewport.size.y <= 0)
        _controller.Input(_router.Clear());
    else
        _controller.Input(_router.Route(_windowInput, {io.MousePos.x, io.MousePos.y}, result.interaction.viewport,
                                        IsWindowFocused(), result.interaction.focused, keyboardBlocked,
                                        pointerBlocked));
    for (const auto& command : commands)
        command();
    _ui.EndFrame();
    if (_windowWanted) { // The finished frame, before the buffers swap
        _windowWanted = false;
        const auto sequence = ++_windowSequence; // Failed attempts also consume their sequence.
        try {
            auto pixels = ReadWindowPixels();
            pixels.sequence = sequence;
            _window = std::make_shared<const ViewportPixels>(std::move(pixels));
        } catch (const std::exception& error) {
            _controller.Log(std::string("Window capture: ") + error.what(), EditorLogLevel::Warning);
            if (_windowMark && _windowMarkSequence && sequence >= _windowMarkSequence) {
                _windowMark.reset();
                _windowMarkSequence = 0;
                _workspace.NotifyContext(std::string("Window mark capture failed: ") + error.what(), true);
            }
        }
    }
}

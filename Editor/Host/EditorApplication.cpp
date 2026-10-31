#include "Editor/Host/EditorApplication.h"
#include <imgui.h>
#include <vector>
#include "Editor/Host/EditorEntryPoint.h"
#include "Input/Internals/InputReader.h"
#include "Platform/Internals/UserData.h"

namespace {
constexpr double SettingsSaveDelay = 0.5;
GameSettings EditorSettings() {
    GameSettings settings;
    settings.applicationId = "EngineEditor";
    settings.persistentOptions = false;
    settings.window.title = "Engine Editor";
    settings.window.width = 1440;
    settings.window.height = 900;
    settings.window.minWidth = 1000;
    settings.window.minHeight = 650;
    settings.window.targetFps = 60;
    settings.window.alwaysRun = true;
    settings.window.maximized = true;
    settings.canvasWidth = 1280;
    settings.canvasHeight = 720;
    settings.background = {20, 22, 28, 255};
    return settings;
}
} // namespace
int RunEngineEditor(const std::filesystem::path& projectFile) {
    EditorApplication application(projectFile);
    application.Run();
    return 0;
}
EditorApplication::EditorApplication(const std::filesystem::path& projectFile)
    : Game(EditorSettings())
    , _controller(projectFile) {
    try {
        if (const auto& project = _controller.Project()) _control.emplace(project->buildDirectory);
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
        _saveAt = GetTime() + SettingsSaveDelay;
        _dirty = true;
    }
    if (!_dirty || (!force && GetTime() < _saveAt)) return;
    if (auto error = _settings->Save(_projectKey, _preferences, _fontSize); !error.empty())
        _controller.Log(error, EditorLogLevel::Warning);
    _dirty = false;
}
void EditorApplication::Update(float realSeconds) {
    (void)realSeconds;
    if (_quit) RequestQuit();
    _controller.Tick();
    if (_control) _control->Tick(_controller);
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
    if (auto opened = _controller.TakeOpenedPreview()) _preferences.Opened(*opened);
    SavePreferences(false);
}
void EditorApplication::Stop() {
    if (_stopped) return;
    _workspace.CaptureLayout(_preferences);
    SavePreferences(true);
    _control.reset();
    _controller.Shutdown();
    _stopped = true;
}
void EditorApplication::DrawWindow() const {
    if (auto frame = _controller.TakeFrame()) {
        const bool timed = _controller.Snapshot().profiler.liveEnabled;
        const auto transferStarted = timed ? std::chrono::steady_clock::now() : std::chrono::steady_clock::time_point{};
        if (!_display.Upload(frame->width, frame->height, frame->rgba, _controller.Generation(), frame->sequence,
                             (PreviewTimestamp() - frame->capturedAt) * 1000))
            _controller.Log("Could not create the preview texture.", EditorLogLevel::Error);
        if (timed)
            _controller.HostTransfer(
                std::chrono::duration<double, std::milli>(std::chrono::steady_clock::now() - transferStarted).count());
    }
    _ui.BeginFrame(_fontSize);
    // Defer mutations until all panels have consumed the same borrowed snapshot.
    std::vector<std::function<void()>> commands;
    bool editorInput = false;
    const EditorUiActions actions{
        [&](EditorCommand command) {
            editorInput = true;
            commands.emplace_back([this, command] { _controller.Execute(command); });
        },
        [&](const std::string& id) {
            editorInput = true;
            commands.emplace_back([this, id] { _controller.Select(id); });
        },
        [&](const std::string& id, const PreviewValue& value) {
            commands.emplace_back([this, id, value] { _controller.Parameter(id, value); });
        },
        [&](const std::string& id) {
            editorInput = true;
            commands.emplace_back([this, id] { _controller.Action(id); });
        },
        [&] { commands.emplace_back([this] { _controller.ClearOutput(); }); },
        [&] { commands.emplace_back([this] { _controller.AcknowledgeOutput(); }); },
        [&](double speed, bool step) {
            editorInput = true;
            commands.emplace_back([this, speed, step] { _controller.Playback(speed, step); });
        },
        [&](EditorProfilerCommand command, Profiler::Recipe recipe, const std::string& file) {
            editorInput = true;
            commands.emplace_back([this, command, recipe, file] { _controller.Profile(command, recipe, file); });
        },
        [&](bool attempt) { commands.emplace_back([this, attempt] { _controller.OpenOutputLog(attempt); }); },
        [&](std::size_t file, const TuningLocation& at, const TuningValue& value) {
            commands.emplace_back([this, file, at, value] { _controller.Tune(file, at, value); });
        }};
    const auto result = _workspace.Draw(_controller.Snapshot(), actions, _display.Image(), _preferences, _fontSize);
    _quit = result.quit;
    const auto& io = ImGui::GetIO();
    const bool popup = ImGui::IsPopupOpen(nullptr, ImGuiPopupFlags_AnyPopupId | ImGuiPopupFlags_AnyPopupLevel);
    const bool keyboardBlocked =
        popup || io.WantTextInput || (ImGui::IsAnyItemActive() && !result.interaction.contentActive);
    const bool pointerBlocked =
        popup || !result.interaction.hovered ||
        (ImGui::IsAnyItemActive() && !result.interaction.contentActive && !_router.OwnsPointer());
    if (editorInput || result.cancelPreviewInput) _workspace.ReleaseInput();
    if (editorInput || result.cancelPreviewInput || result.interaction.viewport.size.x <= 0 ||
        result.interaction.viewport.size.y <= 0)
        _controller.Input(_router.Clear());
    else
        _controller.Input(_router.Route(_windowInput, {io.MousePos.x, io.MousePos.y}, result.interaction.viewport,
                                        IsWindowFocused(), result.interaction.focused, keyboardBlocked,
                                        pointerBlocked));
    for (const auto& command : commands)
        command();
    _ui.EndFrame();
}

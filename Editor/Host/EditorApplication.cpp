#include "Editor/Host/EditorApplication.h"
#include <imgui.h>
#include <algorithm>
#include <vector>
#include "Editor/Host/EditorEntryPoint.h"
#include "Editor/Host/FramePng.h"
#include "Editor/Host/ProjectListLauncher.h"
#include "Editor/Internals/Session/ViewportMark.h"
#include "Input/Internals/InputReader.h"
#include "Platform/Internals/UserData.h"
#include "ToolUi/ToolWindow.h"

namespace {
constexpr double SettingsSaveDelay = 0.5;
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
    if (const auto& project = _controller.Project()) _workspace.SetConsoleDirectory(project->sourceDirectory);
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
        if (!_control) return;
        const Vec2 size{window.width, window.height};
        const double scale =
            std::min(1.0, static_cast<double>(MarkMaxImageSide / std::max(region.width, region.height)));
        const auto image = CaptureFrame(window, size, {region, scale});
        const auto file = _control->WriteCapture(image, {}, {}, "mark");
        const auto snapshot = _controller.Snapshot();
        _workspace.PasteToConsole(
            WindowMarkContext(region, size, file, snapshot.selected ? snapshot.selected->id : std::string()),
            _preferences);
    } catch (const std::exception& error) {
        _controller.Log(std::string("Mark: ") + error.what(), EditorLogLevel::Warning);
    }
}
void EditorApplication::PasteMark(const FloatRect& region) {
    try {
        const auto snapshot = _controller.Snapshot();
        const auto frame = _controller.LatestFrame();
        if (!_control || !frame || !snapshot.selected) return;
        const double scale =
            std::min(1.0, static_cast<double>(MarkMaxImageSide / std::max(region.width, region.height)));
        const auto image = CaptureFrame(*frame, snapshot.canvas, {region, scale});
        const auto file = _control->WriteCapture(image, {}, {}, "mark");
        _workspace.PasteToConsole(MarkContext(*snapshot.selected, snapshot.parameters, _controller.Seconds(),
                                              snapshot.paused, region, snapshot.canvas, file),
                                  _preferences);
    } catch (const std::exception& error) {
        _controller.Log(std::string("Mark: ") + error.what(), EditorLogLevel::Warning);
    }
}
void EditorApplication::Stop() {
    if (_stopped) return;
    _workspace.CaptureLayout(_preferences);
    SavePreferences(true);
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
        [&](const std::string& id, const SceneValue& value) {
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
    if (result.interaction.mark) _mark = result.interaction.mark;
    if (result.windowMark) _windowMark = result.windowMark;
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
        try {
            auto pixels = ReadWindowPixels();
            pixels.sequence = ++_windowSequence;
            _window = std::make_shared<const ViewportPixels>(std::move(pixels));
        } catch (const std::exception& error) {
            _controller.Log(std::string("Window capture: ") + error.what(), EditorLogLevel::Warning);
        }
    }
}

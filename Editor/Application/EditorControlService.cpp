#include "Editor/Application/EditorControlService.h"
#include <windows.h>
#include <algorithm>
#include <cmath>
#include <exception>
#include <format>
#include <fstream>
#include <iterator>
#include <stdexcept>
#include <string>
#include <system_error>
#include "Editor/Internals/Protocol/PixelatorProtocol.h"
#include "Editor/Internals/Protocol/WorkerProtocol.h"
#include "Editor/Internals/Session/AssetLibrary.h"
#include "Editor/Internals/Session/FrameCapture.h"
#include "Editor/Internals/Platform/Windows/WindowsText.h"
#include "Platform/Internals/FileStorage.h"

namespace {
constexpr auto ReadyTimeout = std::chrono::seconds(90);  // Worker start, including a game flow's loading
constexpr auto WorkerTimeout = std::chrono::seconds(15); // Sync and the frames after it
constexpr double SeekWaitPerSecond = 0.5;                // Extra wait per sought second (unthrottled stepping)
constexpr double MaxSeekSeconds = 600.0;
constexpr double StepSeconds = 1.0 / 60.0;
constexpr int MaxSteps = 600;
constexpr double MaxSeekSteps = 600.0 * 240.0;          // Ten minutes at the highest step rate
constexpr std::uint64_t FramesAfterSync = 2;            // The second frame after the answer was drawn after it
constexpr auto AudioTimeout = std::chrono::seconds(10); // Loading a long sound from disk
constexpr std::size_t DefaultLogLimit = 100;
constexpr std::size_t MaxLogLimit = 1000;
constexpr int DefaultGridFrames = 9;
constexpr double GridWidth = 1600.0; // Pixels a frame grid's tiles add up to across, unless a scale is given

std::string ReadText(const std::filesystem::path& file) {
    std::ifstream input(file, std::ios::binary);
    return {std::istreambuf_iterator<char>(input), {}};
}
const char* PhaseName(EditorPhase phase) {
    switch (phase) {
    case EditorPhase::ReadingCatalog: return "reading-catalog";
    case EditorPhase::Starting      : return "starting";
    case EditorPhase::Ready         : return "ready";
    case EditorPhase::Paused        : return "paused";
    case EditorPhase::Building      : return "building";
    case EditorPhase::Stopped       : return "stopped";
    case EditorPhase::Error         : return "error";
    }
    return "stopped";
}
nlohmann::json StatusOf(const EditorController& controller) {
    const auto snapshot = controller.Snapshot();
    nlohmann::json status{{"phase", PhaseName(snapshot.phase)},
                          {"status", snapshot.status},
                          {"ready", snapshot.ready},
                          {"building", snapshot.building},
                          {"scene", snapshot.selected ? nlohmann::json(snapshot.selected->id) : nlohmann::json()}};
    if (snapshot.ready) {
        status["paused"] = snapshot.paused;
        status["speed"] = snapshot.playbackSpeed;
        status["seconds"] = controller.Seconds();
        status["canvas"] = {snapshot.canvas.x, snapshot.canvas.y};
        if (controller.SimulationState().is_object()) status.update(controller.SimulationState());
    }
    return status;
}
void RequireSimulation(const EditorController& controller) {
    if (!controller.SimulationState().is_object() || !controller.SimulationState().contains("step"))
        throw std::runtime_error("The open scene has no simulation");
}
// The step the scene stands on: a seek can give up before its target, for example under slow motion.
std::uint64_t ReachedStep(const EditorController& controller) {
    const auto& state = controller.SimulationState();
    return state.is_object() ? state.value("step", std::uint64_t{0}) : std::uint64_t{0};
}
// Stepping needs time per step: how long a fast replay of 'steps' may take before the wait gives up.
std::chrono::steady_clock::duration StepWait(const EditorController& controller, std::uint64_t steps) {
    const int rate = controller.SimulationState().at("stepsPerSecond").get<int>();
    return WorkerTimeout + std::chrono::duration_cast<std::chrono::steady_clock::duration>(
                               std::chrono::duration<double>(static_cast<double>(steps) * SeekWaitPerSecond / rate));
}
const char* WaveformName(const AudioPreviewState& audio) {
    switch (audio.waveform.state) {
    case WaveformState::Decoding: return "decoding";
    case WaveformState::Ready   : return "ready";
    case WaveformState::Failed  : return "failed";
    }
    return "decoding";
}
std::string Text(const nlohmann::json& arguments, const char* key) {
    const auto found = arguments.find(key);
    if (found == arguments.end() || !found->is_string() || found->get<std::string>().empty())
        throw std::runtime_error(std::format("Missing \"{}\"", key));
    return found->get<std::string>();
}
double Number(const nlohmann::json& arguments, const char* key) {
    const auto found = arguments.find(key);
    if (found == arguments.end() || !found->is_number() || !std::isfinite(found->get<double>()))
        throw std::runtime_error(std::format("\"{}\" must be a number", key));
    return found->get<double>();
}
// A whole number of steps from an argument, checked; used by step --sim, seek --step and frames --steps.
std::uint64_t StepNumber(const nlohmann::json& arguments, const char* key, double minimum) {
    const double value = Number(arguments, key);
    if (value < minimum || value > MaxSeekSteps || std::trunc(value) != value)
        throw std::runtime_error(
            std::format("\"{}\" must be a whole number from {} to {}", key, minimum, MaxSeekSteps));
    return static_cast<std::uint64_t>(value);
}
// The step of a moment the open simulation names now; a refusal lists the names it does know.
double MomentStep(const EditorController& controller, const std::string& name) {
    const auto& state = controller.SimulationState();
    const auto moments = state.find("moments");
    if (moments != state.end()) {
        if (const auto found = moments->find(name); found != moments->end()) return found->get<double>();
        std::string known;
        for (const auto& [moment, step] : moments->items())
            known += (known.empty() ? "" : ", ") + moment;
        throw std::runtime_error(std::format("The simulation names no moment \"{}\" at this step; it knows: {}", name, known));
    }
    throw std::runtime_error("The simulation names no moments; it can list them in Simulation::Moments");
}
const SceneDefinition& Selected(const EditorSnapshot& snapshot) {
    if (!snapshot.selected) throw std::runtime_error("No scene is selected; open one first");
    return *snapshot.selected;
}
constexpr double MaxAudioCaptureSeconds = 600.0;

void RequireReady(const EditorSnapshot& snapshot) {
    if (!snapshot.ready) throw std::runtime_error("The scene is not running (" + snapshot.status + "); open it first");
    if (snapshot.profiler.busy) throw std::runtime_error("A profiler run owns the scene; wait or cancel it");
}
FrameCaptureOptions CaptureOptionsOf(const nlohmann::json& arguments, Vec2 canvas) {
    FrameCaptureOptions options;
    if (arguments.contains("scale")) options.scale = Number(arguments, "scale");
    if (!std::isfinite(options.scale) || options.scale < FrameCaptureMinScale || options.scale > FrameCaptureMaxScale)
        throw std::runtime_error(
            std::format("Scale must be between {} and {}", FrameCaptureMinScale, FrameCaptureMaxScale));
    if (const auto crop = arguments.find("crop"); crop != arguments.end()) {
        if (!crop->is_array() || crop->size() != 4 ||
            !std::ranges::all_of(*crop, [](const nlohmann::json& value) { return value.is_number(); }))
            throw std::runtime_error("\"crop\" must be [x, y, width, height] in canvas units");
        const FloatRect area{(*crop)[0].get<float>(), (*crop)[1].get<float>(), (*crop)[2].get<float>(),
                             (*crop)[3].get<float>()};
        if (area.width <= 0.0f || area.height <= 0.0f || area.x < 0.0f || area.y < 0.0f ||
            area.Right() > static_cast<float>(canvas.x) || area.Bottom() > static_cast<float>(canvas.y))
            throw std::runtime_error(std::format("Crop must lie inside the {}x{} image", canvas.x, canvas.y));
        options.crop = area;
    }
    return options;
}
std::string Describe(const SceneValue& value) { return ControlValue(value).dump(); }
double Now() { return std::chrono::duration<double>(std::chrono::steady_clock::now().time_since_epoch()).count(); }
constexpr const char* LookFirst =
    "so the scene may not be as you left it. Check it with: python Engine ctl status, then retry";
} // namespace

EditorControlService::EditorControlService(const std::filesystem::path& buildDirectory, EditorCaptureHost host)
    : _announcement(buildDirectory / EditorControlFile)
    , _captures(buildDirectory / "editor-captures")
    , _host(std::move(host)) {
    std::filesystem::create_directories(buildDirectory);
    (void)WriteBinaryFileAtomic(_announcement, EncodeControlAnnouncement({_pipe.Name(), GetCurrentProcessId()}));
}
EditorControlService::~EditorControlService() {
    try { // Another editor of the same project may have announced itself since; leave its file alone.
        if (DecodeControlAnnouncement(ReadText(_announcement)).pipe != _pipe.Name()) return;
        std::error_code ignored;
        std::filesystem::remove(_announcement, ignored);
    } catch (...) {
    }
}
void EditorControlService::Tick(EditorController& controller) {
    try {
        for (const auto& line : _pipe.Poll())
            Handle(line, controller);
        Poll(controller);
        if (_watch) {
            if (!_pipe.Connected(_watch->from.client)) {
                _watch.reset(); // The client left; the attempt goes on without it
            } else {
                _current = _watch->from;
                controller.SetControlSource(_current.session);
                Stream(controller);
            }
        }
    } catch (const std::exception& error) {
        _watch.reset();
        _pending.clear();
        controller.Log(std::string("Control endpoint: ") + error.what(), EditorLogLevel::Warning);
    }
    // A held command still being answered (a build, a long seek) keeps its session from looking idle.
    const double now = Now();
    for (const auto& [client, pending] : _pending)
        if (pending.from.holds) _lease.Touch(pending.from.session, now);
    if (_watch && _watch->from.holds) _lease.Touch(_watch->from.session, now);
    UpdateReport();
}
bool EditorControlService::Busy(std::uint64_t client) const {
    return _pending.contains(client) || (_watch && _watch->from.client == client);
}
void EditorControlService::Handle(const EditorControlPipe::Line& line, EditorController& controller) {
    if (Busy(line.client)) {
        _pipe.Send(line.client, EncodeControlError("The editor is still answering this connection's last request"));
        return;
    }
    _current = {line.client, {}, false};
    try {
        const auto request = DecodeControlRequest(line.text);
        _current.session = request.session;
        controller.SetControlSource(request.session);
        if (auto answer = Run(request, controller)) {
            if (_current.holds) _lease.Touch(_current.session, Now());
            _pipe.Send(line.client, EncodeControlReply(std::move(*answer)));
        }
    } catch (const std::exception& error) {
        _pending.erase(line.client);
        if (_watch && _watch->from.client == line.client) _watch.reset();
        _pipe.Send(line.client, EncodeControlError(error.what()));
    }
}
void EditorControlService::Poll(EditorController& controller) {
    const auto now = std::chrono::steady_clock::now();
    for (auto entry = _pending.begin(); entry != _pending.end();) {
        auto& pending = entry->second;
        const auto client = pending.from.client;
        if (!_pipe.Connected(client)) { // The client left; what it started goes on
            entry = _pending.erase(entry);
            continue;
        }
        _current = pending.from;
        controller.SetControlSource(pending.from.session);
        std::optional<std::string> reply;
        try {
            if (auto answer = pending.poll(controller))
                reply = EncodeControlReply(std::move(*answer));
            else if (now >= pending.deadline)
                reply = EncodeControlError("Timed out waiting for " + pending.waitingFor);
        } catch (const std::exception& error) {
            reply = EncodeControlError(error.what());
        }
        if (!reply) {
            ++entry;
            continue;
        }
        if (pending.from.holds) _lease.Touch(pending.from.session, Now());
        entry = _pending.erase(entry);
        _pipe.Send(client, *reply);
    }
}
void EditorControlService::Await(std::string waitingFor, std::chrono::steady_clock::duration timeout,
                                 std::function<std::optional<nlohmann::json>(EditorController&)> poll) {
    _pending[_current.client] =
        Pending{_current, std::chrono::steady_clock::now() + timeout, std::move(waitingFor), std::move(poll)};
}
void EditorControlService::TakeHold(Requester& from) {
    const auto running = std::ranges::find_if(_pending, [](const auto& entry) { return entry.second.from.holds; });
    if (running != _pending.end() || (_watch && _watch->from.holds)) {
        const auto& other = running != _pending.end() ? running->second.from.session : _watch->from.session;
        throw std::runtime_error(
            std::format("The editor is still carrying out a request from {}; retry when it has answered",
                        other == from.session ? std::string("you") : NameOf(other)));
    }
    if (const auto refused = _lease.Take(from.session, Now())) {
        switch (refused->refusal) {
        case ControlLease::Refusal::Held:
            throw std::runtime_error(std::format(
                "{} is using the editor (last command {:.0f} s ago); it is free after {:.0f} s without commands. "
                "Meanwhile use commands that only look (status, inspect, logs) or work that needs no editor, then "
                "retry",
                NameOf(refused->other), refused->idle, ControlLease::IdleSeconds));
        case ControlLease::Refusal::Developer:
            throw std::runtime_error(std::string("The developer used the editor since your last command, ") +
                                     LookFirst);
        case ControlLease::Refusal::Taken:
            throw std::runtime_error(std::format("{} used the editor after you were idle for {:.0f} s, {}",
                                                 NameOf(refused->other), ControlLease::IdleSeconds, LookFirst));
        }
    }
    from.holds = true;
}
std::string EditorControlService::NameOf(const std::string& session) const {
    if (session.empty()) return "An agent outside the editor's Console";
    if (const auto found = _sessionTitles.find(session); found != _sessionTitles.end()) return found->second;
    return "Console session " + session;
}
void EditorControlService::SetConsoleSessions(std::map<std::string, std::string> titles) {
    // A session that was live and is not any more (closed, or its program exited) gives up the editor quietly.
    for (const auto& [session, title] : _sessionTitles)
        if (!titles.contains(session)) _lease.Forget(session);
    _sessionTitles = std::move(titles);
}
void EditorControlService::UpdateReport() {
    _report.control = _agentControl;
    _report.visualChecks = _visualChecks;
    _report.hold.reset();
    if (const auto hold = _lease.Holder(Now())) _report.hold = AgentEditorHold{hold->client, hold->idle};
}
void EditorControlService::Note(const EditorControlRequest& request, EditorController& controller) {
    auto note = DecodeAgentNote(request.arguments);
    note.session = request.session;
    auto& session = _report.sessions[note.session];
    session.note = note;
    session.noteTime = std::chrono::steady_clock::now();
    if (note.text.empty()) return;
    if (note.live) {
        session.tools.push_back({std::chrono::system_clock::now(), note.text});
        ++session.toolCount;
        if (session.tools.size() > AgentToolsKept) session.tools.pop_front();
        return;
    }
    controller.RecordControl(
        {std::chrono::system_clock::now(), note.text, {}, {}, {}, {}, EditorControlEventKind::Note});
}
void EditorControlService::AwaitWorker(EditorController& controller, std::chrono::steady_clock::duration timeout,
                                       Answer answer) {
    const auto request = controller.Sync();
    Await("the scene to apply the request", timeout,
          [request, answer = std::move(answer)](EditorController& session) -> std::optional<nlohmann::json> {
              const auto snapshot = session.Snapshot();
              if (!snapshot.ready) throw std::runtime_error("The scene stopped: " + snapshot.status);
              const auto& sync = session.LastSync();
              // Another client's later sync answers for this request too: the worker applies them in order.
              if (sync.request < request || sync.frames < FramesAfterSync) return std::nullopt;
              return answer(session);
          });
}
void EditorControlService::AwaitReady(const std::string& id) {
    Await("the scene to start", ReadyTimeout, [id](EditorController& session) -> std::optional<nlohmann::json> {
        const auto snapshot = session.Snapshot();
        if (snapshot.phase == EditorPhase::Error) throw std::runtime_error("The scene failed: " + snapshot.status);
        if (snapshot.phase == EditorPhase::Stopped) throw std::runtime_error("The scene stopped: " + snapshot.status);
        if (!snapshot.ready || !snapshot.selected || snapshot.selected->id != id) return std::nullopt;
        return StatusOf(session);
    });
}
std::optional<nlohmann::json> EditorControlService::Run(const EditorControlRequest& request,
                                                        EditorController& controller) {
    const auto& arguments = request.arguments;
    const auto snapshot = controller.Snapshot();
    if (!_agentControl && ControlCommandChangesEditor(request.command))
        throw std::runtime_error("Agent control is off in the editor (Agent menu); ask the developer to turn it on");
    if (!_visualChecks && ControlCommandCaptures(request.command))
        throw std::runtime_error(std::string(VisualChecksOffText));
    if (ControlCommandNeedsHold(request.command)) TakeHold(_current);
    switch (request.command) {
    case EditorControlCommand::Refresh: Refresh(controller); return std::nullopt;
    case EditorControlCommand::Status : {
        auto status = StatusOf(controller);
        status["control"] = _agentControl;
        status["visualChecks"] = _visualChecks;
        if (const auto hold = _lease.Holder(Now()))
            status["hold"] = {{"by", NameOf(hold->client)},
                              {"yours", hold->client == request.session},
                              {"idleSeconds", std::round(hold->idle)}};
        return status;
    }
    case EditorControlCommand::List: {
        auto scenes = nlohmann::json::array();
        for (const auto& definition : snapshot.catalog)
            scenes.push_back(ControlCatalogEntry(definition, controller.Values(definition.id)));
        return nlohmann::json{
            {"scenes", std::move(scenes)},
            {"selected", snapshot.selected ? nlohmann::json(snapshot.selected->id) : nlohmann::json()}};
    }
    case EditorControlCommand::Open: {
        const auto id = Text(arguments, "id");
        const auto found = std::ranges::find(snapshot.catalog, id, &SceneDefinition::id);
        if (found == snapshot.catalog.end())
            throw std::runtime_error("Unknown scene '" + id + "'; `ctl list` shows the catalog");
        if (snapshot.ready && snapshot.selected && snapshot.selected->id == id) return StatusOf(controller);
        if (snapshot.busy) throw std::runtime_error("The editor is busy (" + snapshot.status + ")");
        controller.Select(id);
        const auto capabilities = controller.Snapshot().capabilities;
        if (!capabilities.Can(EditorCommand::Open))
            throw std::runtime_error(std::string("Cannot open: ") + capabilities.Reason(EditorCommand::Open));
        controller.Execute(EditorCommand::Open);
        controller.Notify("Opened " + found->name);
        AwaitReady(id);
        return std::nullopt;
    }
    case EditorControlCommand::Set: {
        const auto& definition = Selected(snapshot);
        if (snapshot.busy) throw std::runtime_error("The editor is busy (" + snapshot.status + ")");
        const auto values = arguments.find("values");
        if (values == arguments.end() || !values->is_object() || values->empty())
            throw std::runtime_error("\"values\" must be an object of parameter id → value");
        std::vector<std::pair<std::string, SceneValue>> changes;
        for (const auto& [id, value] : values->items()) {
            const auto parameter = std::ranges::find(definition.parameters, id, &SceneParameter::id);
            if (parameter == definition.parameters.end())
                throw std::runtime_error("'" + definition.id + "' has no parameter '" + id + "'");
            auto converted = ControlValueOf(*parameter, value);
            if (const auto error = ValidateSceneValue(*parameter, converted); !error.empty())
                throw std::runtime_error(error);
            changes.emplace_back(id, std::move(converted));
        }
        std::string notice = "Set";
        for (const auto& [id, value] : changes) {
            const auto before = snapshot.parameters.find(id);
            const auto previous =
                before == snapshot.parameters.end() ? std::optional<SceneValue>{} : std::optional(before->second);
            controller.RecordControl(
                {std::chrono::system_clock::now(),
                 id + (previous ? ": " + Describe(*previous) + " \xE2\x86\x92 " : ": ") + Describe(value),
                 definition.id,
                 id,
                 previous,
                 {},
                 EditorControlEventKind::Change});
            controller.Parameter(id, value);
            notice += " " + id + "=" + Describe(value);
        }
        controller.Notify(notice, false);
        const auto answer = [definition = definition.id, changes](EditorController& session) {
            const auto current = session.Values(definition);
            nlohmann::json applied = nlohmann::json::object();
            for (const auto& [id, value] : changes) {
                (void)value;
                if (const auto found = current.find(id); found != current.end())
                    applied[id] = ControlValue(found->second);
            }
            return nlohmann::json{{"scene", definition}, {"parameters", applied}};
        };
        if (!snapshot.ready) return answer(controller); // Stored for the next start
        AwaitWorker(controller, WorkerTimeout, answer);
        return std::nullopt;
    }
    case EditorControlCommand::Action: {
        RequireReady(snapshot);
        const auto& definition = Selected(snapshot);
        const auto id = Text(arguments, "id");
        const auto action = std::ranges::find(definition.actions, id, &SceneAction::id);
        if (action == definition.actions.end())
            throw std::runtime_error("'" + definition.id + "' has no action '" + id + "'");
        if (std::ranges::find(snapshot.inspection.disabledActions, id) != snapshot.inspection.disabledActions.end())
            throw std::runtime_error("Action '" + id + "' is disabled right now");
        controller.Action(id);
        controller.Notify("Action " + action->label);
        AwaitWorker(controller, WorkerTimeout, StatusOf);
        return std::nullopt;
    }
    case EditorControlCommand::Pause:
    case EditorControlCommand::Play : {
        RequireReady(snapshot);
        const bool pause = request.command == EditorControlCommand::Pause;
        controller.SetPaused(pause);
        controller.Notify(pause ? "Paused" : "Playing");
        AwaitWorker(controller, WorkerTimeout, StatusOf);
        return std::nullopt;
    }
    case EditorControlCommand::Step: {
        RequireReady(snapshot);
        if (arguments.value("sim", false)) {
            RequireSimulation(controller);
            const std::uint64_t steps = arguments.contains("frames") ? StepNumber(arguments, "frames", 1.0) : 1;
            controller.SeekStep(steps, true);
            controller.Notify(std::format("Step {} simulation step(s)", steps));
            AwaitWorker(controller, StepWait(controller, steps), StatusOf);
            return std::nullopt;
        }
        const double frames = arguments.contains("frames") ? Number(arguments, "frames") : 1.0;
        if (frames < 1.0 || frames > MaxSteps || std::trunc(frames) != frames)
            throw std::runtime_error(std::format("\"frames\" must be a whole number from 1 to {}", MaxSteps));
        controller.Seek(frames * StepSeconds, true);
        controller.Notify(std::format("Step {} frame(s)", frames));
        AwaitWorker(controller, WorkerTimeout, StatusOf);
        return std::nullopt;
    }
    case EditorControlCommand::Speed: {
        RequireReady(snapshot);
        const double speed = Number(arguments, "speed");
        if (!ValidPlaybackSpeed(speed))
            throw std::runtime_error(
                std::format("Speed must be between {} and {}", WorkerMinPlaybackSpeed, WorkerMaxPlaybackSpeed));
        controller.Playback(speed, false);
        controller.Notify(std::format("Speed {}x", speed));
        AwaitWorker(controller, WorkerTimeout, StatusOf);
        return std::nullopt;
    }
    case EditorControlCommand::Reset: {
        RequireReady(snapshot);
        const auto& definition = Selected(snapshot);
        if (!snapshot.capabilities.Can(EditorCommand::Reset))
            throw std::runtime_error(std::string("Cannot reset: ") +
                                     snapshot.capabilities.Reason(EditorCommand::Reset));
        controller.Execute(EditorCommand::Reset);
        controller.Notify("Reset " + definition.name);
        AwaitReady(definition.id);
        return std::nullopt;
    }
    case EditorControlCommand::Seek: {
        RequireReady(snapshot);
        if (arguments.contains("step") || arguments.contains("moment")) {
            RequireSimulation(controller);
            const auto step = StepNumber(
                arguments.contains("moment")
                    ? nlohmann::json{{"step", MomentStep(controller, Text(arguments, "moment"))}}
                    : arguments,
                "step", 0.0);
            controller.SeekStep(step);
            controller.Notify(std::format("Seek step {}", step));
            const auto travel = std::max(step, controller.SimulationState().at("step").get<std::uint64_t>());
            AwaitWorker(controller, StepWait(controller, travel), StatusOf);
            return std::nullopt;
        }
        const double seconds = Number(arguments, "seconds");
        if (seconds < 0.0 || seconds > MaxSeekSeconds)
            throw std::runtime_error(std::format("\"seconds\" must be between 0 and {}", MaxSeekSeconds));
        controller.Seek(seconds);
        controller.Notify(std::format("Seek {:.2f} s", seconds));
        const auto travel = std::max(seconds, controller.Seconds());
        AwaitWorker(controller,
                    WorkerTimeout + std::chrono::duration_cast<std::chrono::steady_clock::duration>(
                                        std::chrono::duration<double>(travel * SeekWaitPerSecond)),
                    StatusOf);
        return std::nullopt;
    }
    case EditorControlCommand::Replay: {
        RequireReady(snapshot);
        RequireSimulation(controller);
        const auto action = Text(arguments, "action");
        if (action != "load" && action != "save") throw std::runtime_error("\"action\" must be load or save");
        controller.Replay(action, Text(arguments, "path"));
        const std::string done = action == "load" ? "Replay loaded" : "Replay saved";
        AwaitWorker(controller, WorkerTimeout, [done](EditorController& session) {
            auto status = StatusOf(session);
            if (status.contains("error")) throw std::runtime_error(status.at("error").get<std::string>());
            session.Notify(done);
            return status;
        });
        return std::nullopt;
    }
    case EditorControlCommand::Screenshot: {
        if (arguments.value("window", false)) {
            WindowShot(arguments);
            return std::nullopt;
        }
        RequireReady(snapshot);
        (void)CaptureOptionsOf(arguments, snapshot.canvas); // Report bad options before waiting
        if (arguments.contains("path") && Text(arguments, "path").empty())
            throw std::runtime_error("\"path\" must not be empty");
        AwaitWorker(controller, WorkerTimeout,
                    [this, arguments](EditorController& session) { return Screenshot(arguments, session); });
        return std::nullopt;
    }
    case EditorControlCommand::Frames:
        RequireReady(snapshot);
        Frames(arguments, controller);
        return std::nullopt;
    case EditorControlCommand::Inspect: {
        RequireReady(snapshot);
        AwaitWorker(controller, WorkerTimeout, [](EditorController& session) {
            auto status = StatusOf(session);
            status["inspection"] = EncodeSceneInspection(session.Snapshot().inspection);
            return status;
        });
        return std::nullopt;
    }
    case EditorControlCommand::Logs: {
        const auto since = arguments.value("since", std::uint64_t{0});
        const auto limit = std::min(arguments.value("limit", DefaultLogLimit), MaxLogLimit);
        const auto level = arguments.value("level", std::string("info"));
        const auto minimum = level == "error"     ? EditorLogLevel::Error
                             : level == "warning" ? EditorLogLevel::Warning
                             : level == "info"    ? EditorLogLevel::Info
                                                  : throw std::runtime_error("\"level\" is info, warning or error");
        std::vector<nlohmann::json> entries;
        std::uint64_t last = since;
        for (const auto& entry : controller.Diagnostics().Entries()) {
            last = std::max(last, entry.sequence);
            if (entry.sequence > since && entry.level >= minimum) entries.push_back(ControlLogEntry(entry));
        }
        if (entries.size() > limit) entries.erase(entries.begin(), entries.end() - static_cast<std::ptrdiff_t>(limit));
        return nlohmann::json{{"entries", entries}, {"last", last}};
    }
    case EditorControlCommand::Note: {
        Note(request, controller);
        const auto& note = *_report.sessions.at(request.session).note;
        return nlohmann::json{
            {"state", AgentActivityName(note.state)}, {"text", note.text}, {"control", _agentControl}};
    }
    case EditorControlCommand::Release     : return nlohmann::json{{"released", _lease.Release(request.session)}};
    case EditorControlCommand::Pixels      : Pixels(arguments); return std::nullopt;
    case EditorControlCommand::Audio       : return AudioStatus(controller);
    case EditorControlCommand::AudioControl:
        if (Text(arguments, "action") == "capture") {
            RequireReady(snapshot);
            const double seconds = Number(arguments, "seconds");
            if (seconds <= 0.0 || seconds > MaxAudioCaptureSeconds)
                throw std::runtime_error(std::format("\"seconds\" must be above 0 and at most {}", MaxAudioCaptureSeconds));
            controller.CaptureAudio(Text(arguments, "file"), seconds);
            AwaitWorker(controller, WorkerTimeout, StatusOf);
            return std::nullopt;
        }
        AudioControl(arguments);
        return std::nullopt;
    case EditorControlCommand::Count       : break;
    }
    throw std::runtime_error("Unsupported command");
}
nlohmann::json EditorControlService::AudioStatus(const EditorController& controller) const {
    if (!_audio.state) throw std::runtime_error("This editor has no Audio Preview");
    const auto audio = _audio.state();
    nlohmann::json status{{"loaded", !audio.file.empty()}, {"loop", audio.loop}, {"volume", audio.volume}};
    const auto snapshot = controller.Snapshot();
    const auto* usage = snapshot.audioUsage;
    status["catalog"] = usage && usage->reported; // False until a worker has run: no sound counts as unused yet
    if (audio.file.empty()) {
        status["next"] = "No sound loaded; play one with: python Engine ctl audio play FILE";
        return status;
    }
    status["file"] = WorkerUtf8(audio.file.wstring());
    status["playing"] = audio.playing;
    status["paused"] = audio.paused;
    status["position"] = audio.position;
    status["length"] = audio.length;
    status["waveform"] = WaveformName(audio);
    if (!audio.error.empty()) status["error"] = audio.error;
    const auto& project = controller.Project();
    if (!project) return status;
    const auto asset = AssetPathOf(audio.file, project->engineDirectory, project->assetsDirectory);
    if (asset.empty()) return status;
    status["asset"] = WorkerUtf8(asset.generic_wstring());
    if (!usage || !usage->reported) return status;
    auto uses = nlohmann::json::array();
    for (const auto* use : AudioUsesOf(*usage, asset))
        uses.push_back({{"kind", use->kind == AudioUseKind::Music ? "music" : "effect"},
                        {"id", use->id},
                        {"name", use->name},
                        {"label", AudioUseLabel(*use)}});
    status["uses"] = std::move(uses);
    return status;
}
void EditorControlService::AudioControl(const nlohmann::json& arguments) {
    if (!_audio.request || !_audio.state) throw std::runtime_error("This editor has no Audio Preview");
    const auto audio = _audio.state();
    const auto action = Text(arguments, "action");
    AudioPreviewRequest request{AudioPreviewAction::Stop, audio.file, audio.loop, audio.volume};
    const bool loaded = !audio.file.empty();
    const auto requireLoaded = [&] {
        if (!loaded) throw std::runtime_error("No sound is loaded; use: python Engine ctl audio play FILE");
    };
    bool play = false; // Starts a sound: its load error fails the request
    std::uint64_t until = 0;
    const auto seek = [&](double seconds) {
        if (seconds < 0) throw std::runtime_error("\"seconds\" must not be negative");
        until =
            _audio.request({AudioPreviewAction::Seek, {}, request.loop, request.volume, static_cast<float>(seconds)});
    };
    if (action == "play") {
        const std::filesystem::path file(WorkerWide(Text(arguments, "file")));
        if (!file.is_absolute()) throw std::runtime_error("\"file\" must be absolute");
        request = {AudioPreviewAction::Play, file.lexically_normal(), arguments.value("loop", audio.loop),
                   audio.volume};
        until = _audio.request(request);
        play = true;
        if (arguments.contains("at")) seek(Number(arguments, "at"));
    } else if (action == "pause" || action == "resume" || action == "stop") {
        requireLoaded();
        request.action = action == "pause"    ? AudioPreviewAction::Pause
                         : action == "resume" ? AudioPreviewAction::Resume
                                              : AudioPreviewAction::Stop;
        until = _audio.request(request);
    } else if (action == "seek") {
        requireLoaded();
        const double seconds = Number(arguments, "seconds");
        if (!audio.playing && !audio.paused) { // Like a click on the timeline: a stopped sound starts there
            request.action = AudioPreviewAction::Play;
            until = _audio.request(request);
            play = true;
        }
        seek(seconds);
    } else if (action == "volume") {
        const double volume = Number(arguments, "volume");
        if (volume < 0 || volume > 1) throw std::runtime_error("\"volume\" must be between 0 and 1");
        request.action = AudioPreviewAction::Volume;
        request.volume = static_cast<float>(volume);
        until = _audio.request(request);
    } else if (action == "loop") {
        const auto on = arguments.find("on");
        if (on == arguments.end() || !on->is_boolean()) throw std::runtime_error("\"on\" must be true or false");
        request.action = AudioPreviewAction::Loop;
        request.loop = on->get<bool>();
        until = _audio.request(request);
    } else {
        throw std::runtime_error("\"action\" is play, pause, resume, stop, seek, volume or loop");
    }
    Await("the sound preview", AudioTimeout,
          [this, until, play](EditorController& session) -> std::optional<nlohmann::json> {
              const auto now = _audio.state();
              if (now.handled < until) return std::nullopt;
              if (play && !now.error.empty()) throw std::runtime_error("Sound preview: " + now.error);
              return AudioStatus(session);
          });
}
std::filesystem::path EditorControlService::WriteCapture(const ViewportPixels& image,
                                                         const std::vector<FrameCaptureLabel>& labels,
                                                         const std::string& path, const std::string& prefix) {
    const auto png = _host.encodePng(image, labels);
    std::filesystem::path file =
        path.empty() ? NewCapturePath(_captures, prefix) : std::filesystem::path(WorkerWide(path));
    if (!file.is_absolute()) throw std::runtime_error("\"path\" must be absolute");
    file = file.lexically_normal().make_preferred();
    std::filesystem::create_directories(file.parent_path());
    if (!WriteBinaryFileAtomic(file, {reinterpret_cast<const char*>(png.data()), png.size()}))
        throw std::runtime_error("Could not write " + WorkerUtf8(file.generic_wstring()));
    if (path.empty()) PruneCaptures(_captures, FrameCaptureKeep);
    return file;
}
nlohmann::json EditorControlService::Saved(const std::filesystem::path& file, const ViewportPixels& image,
                                           EditorController& controller) {
    controller.RecordControl({std::chrono::system_clock::now(),
                              "Screenshot " + WorkerUtf8(file.filename().wstring()),
                              {},
                              {},
                              {},
                              file,
                              EditorControlEventKind::Capture});
    auto status = StatusOf(controller);
    status["path"] = WorkerUtf8(file.wstring());
    status["width"] = image.width;
    status["height"] = image.height;
    return status;
}
nlohmann::json EditorControlService::Screenshot(const nlohmann::json& arguments, EditorController& controller) {
    const auto snapshot = controller.Snapshot();
    const auto frame = controller.LatestFrame();
    if (!frame) throw std::runtime_error("No frame to capture");
    const auto captured = CaptureFrame(*frame, snapshot.canvas, CaptureOptionsOf(arguments, snapshot.canvas));
    return Saved(WriteCapture(captured, {}, arguments.value("path", std::string()), "shot"), captured, controller);
}
void EditorControlService::WindowShot(const nlohmann::json& arguments) {
    if (arguments.contains("scale")) (void)Number(arguments, "scale");
    if (arguments.contains("path") && Text(arguments, "path").empty())
        throw std::runtime_error("\"path\" must not be empty");
    const auto sequence = _host.requestWindow();
    Await("the editor window", WorkerTimeout,
          [this, sequence, arguments](EditorController& session) -> std::optional<nlohmann::json> {
              const auto window = _host.window(sequence);
              if (!window) return std::nullopt;
              const Vec2 size{window->width, window->height}; // Crop in window pixels
              const auto captured = CaptureFrame(*window, size, CaptureOptionsOf(arguments, size));
              auto status = Saved(WriteCapture(captured, {}, arguments.value("path", std::string()), "window"),
                                  captured, session);
              status["window"] = {window->width, window->height};
              return status;
          });
}
void EditorControlService::Pixels(const nlohmann::json& arguments) {
    const auto request = DecodePixelatorRequest(arguments); // Report bad options before waiting
    if (arguments.contains("path") && Text(arguments, "path").empty())
        throw std::runtime_error("\"path\" must not be empty");
    Await("the pixel graphic from the scene", WorkerTimeout,
          [this, request, arguments](EditorController& session) -> std::optional<nlohmann::json> {
              if (session.PixelsPending()) return std::nullopt;
              const auto pixels = session.Pixels();
              if (!pixels)
                  throw std::runtime_error("No pixel graphic is open; open a graphic whose GraphicAsset has pixels: "
                                           "python Engine ctl open asset:Graphics/...");
              if (!pixels->error.empty()) throw std::runtime_error(pixels->error);
              if (request.frame >= pixels->frames.size())
                  throw std::runtime_error(
                      std::format("\"frame\" must be below {} for {}", pixels->frames.size(), pixels->id));
              const auto& frame = pixels->frames[request.frame];
              const auto& image = pixels->images[frame.image];
              if (image.Empty()) throw std::runtime_error(std::format("Frame {} has no pixels", request.frame));
              auto options = request.picture;
              options.zoom = PixelatorPictureZoom({image.width, image.height}, options.zoom);
              const auto picture = ComposePixelatorPicture(image, pixels->palettes[frame.palette], options);
              auto status =
                  Saved(WriteCapture(picture, {}, arguments.value("path", std::string()), "pixels"), picture, session);
              status.update(PixelatorSummary(*pixels, request.frame));
              status["zoom"] = options.zoom;
              status["view"] = PixelatorViewName(options.view);
              return status;
          });
}
void EditorControlService::Frames(const nlohmann::json& arguments, EditorController& controller) {
    const auto snapshot = controller.Snapshot();
    const double from = Number(arguments, "from"), to = Number(arguments, "to");
    const bool steps = arguments.value("steps", false);
    if (steps) RequireSimulation(controller);
    const double limit = steps ? MaxSeekSteps : MaxSeekSeconds;
    if (from < 0.0 || to <= from || to > limit)
        throw std::runtime_error(std::format("Need 0 <= from < to <= {}", limit));
    const int count = arguments.value("count", DefaultGridFrames);
    if (count < 2 || count > FrameGridMaxFrames)
        throw std::runtime_error(std::format("\"count\" must be from 2 to {}", FrameGridMaxFrames));
    const int columns = arguments.value("columns", static_cast<int>(std::ceil(std::sqrt(static_cast<double>(count)))));
    if (columns < 1 || columns > count) throw std::runtime_error("\"columns\" must be from 1 to count");
    auto options = CaptureOptionsOf(arguments, snapshot.canvas);
    if (!arguments.contains("scale")) { // Fit the grid in about one canvas width
        const double area = options.crop ? options.crop->width : snapshot.canvas.x;
        options.scale = std::clamp(GridWidth / (columns * area), FrameCaptureMinScale, 1.0);
    }
    if (arguments.contains("path") && Text(arguments, "path").empty())
        throw std::runtime_error("\"path\" must not be empty");
    struct Run {
        std::vector<double> times;
        std::vector<ViewportPixels> tiles;
        std::vector<std::string> labels;
        std::uint64_t request = 0;
    };
    auto run = std::make_shared<Run>();
    for (int i = 0; i < count; ++i)
        run->times.push_back(steps ? std::round(from + (to - from) * i / (count - 1))
                                   : from + (to - from) * i / (count - 1));
    const auto shoot = [run, steps](EditorController& session) {
        const double target = run->times[run->tiles.size()];
        if (steps)
            session.SeekStep(static_cast<std::uint64_t>(target));
        else
            session.Seek(target);
        run->request = session.Sync();
    };
    shoot(controller);
    controller.Notify(steps ? std::format("Frames steps {:.0f}-{:.0f} ({})", from, to, count)
                            : std::format("Frames {:.2f}-{:.2f} s ({})", from, to, count));
    const double travelSeconds = steps ? to / controller.SimulationState().at("stepsPerSecond").get<int>() : to;
    const auto timeout =
        WorkerTimeout + std::chrono::duration_cast<std::chrono::steady_clock::duration>(
                            std::chrono::duration<double>(count * 2.0 + travelSeconds * SeekWaitPerSecond));
    Await("the frames", timeout,
          [this, run, shoot, options, columns, arguments,
           steps](EditorController& session) -> std::optional<nlohmann::json> {
              const auto current = session.Snapshot();
              if (!current.ready) throw std::runtime_error("The scene stopped: " + current.status);
              const auto& sync = session.LastSync();
              if (sync.request < run->request || sync.frames < FramesAfterSync) return std::nullopt;
              const auto frame = session.LatestFrame();
              if (!frame) throw std::runtime_error("No frame to capture");
              run->tiles.push_back(CaptureFrame(*frame, current.canvas, options));
              run->labels.push_back(steps ? std::format("step {}", ReachedStep(session))
                                          : std::format("{:.2f}s", sync.seconds));
              if (run->tiles.size() < run->times.size()) {
                  shoot(session);
                  return std::nullopt;
              }
              const auto grid = ComposeFrameGrid(run->tiles, run->labels, columns);
              auto status =
                  Saved(WriteCapture(grid.image, grid.labels, arguments.value("path", std::string()), "frames"),
                        grid.image, session);
              status["frames"] = run->times;
              status["columns"] = columns;
              return status;
          });
}
void EditorControlService::Refresh(EditorController& controller) {
    if (controller.Diagnostics().Attempt().result != EditorBuildResult::Running) {
        const auto capabilities = controller.Snapshot().capabilities;
        if (!capabilities.Can(EditorCommand::Refresh))
            throw std::runtime_error(std::string("Build & Refresh is unavailable: ") +
                                     capabilities.Reason(EditorCommand::Refresh));
        controller.Execute(EditorCommand::Refresh);
        controller.MarkControlAttempt();
        controller.Notify("Build & Refresh");
    }
    _watch = Watch{_current, controller.Diagnostics().Attempt().id, 0};
    Stream(controller);
}
void EditorControlService::Stream(EditorController& controller) {
    const auto& diagnostics = controller.Diagnostics();
    const auto& attempt = diagnostics.Attempt();
    if (attempt.id != _watch->attempt) {
        _watch.reset();
        return;
    }
    for (const auto& entry : diagnostics.AttemptEntries()) {
        if (entry.sequence <= _watch->sentSequence) continue;
        _pipe.Send(_watch->from.client, EncodeControlLog(entry));
        _watch->sentSequence = entry.sequence;
    }
    if (attempt.result == EditorBuildResult::Running || attempt.result == EditorBuildResult::None) return;
    _pipe.Send(_watch->from.client, EncodeControlResult(attempt.result));
    if (attempt.result == EditorBuildResult::BuildFailed || attempt.result == EditorBuildResult::RefreshFailed)
        controller.Notify("Build & Refresh failed; see Output");
    _watch.reset();
}

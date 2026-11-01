#pragma once
#include <nlohmann/json.hpp>
#include <cstddef>
#include <cstdint>
#include <string>
#include <vector>
#include "Audio/AudioSystem.h"
#include "Editor/Api/SceneRegistry.h"
#include "Editor/Internals/Session/AudioUsage.h"
#include "Input/Input.h"

inline constexpr std::uint32_t WorkerProtocolVersion = 12;
inline constexpr std::size_t WorkerMaxInputPoints = 32;
inline constexpr std::size_t WorkerMaxMessageBytes = 1024 * 1024;
inline constexpr std::size_t WorkerMaxQueuedMessages = 256;
inline constexpr double WorkerMinPlaybackSpeed = 0.1;
inline constexpr double WorkerMaxPlaybackSpeed = 4.0;

struct WorkerMessage {
    std::string session;
    std::uint64_t generation = 0;
    std::uint64_t request = 0;
    std::string type;
    nlohmann::json data = nlohmann::json::object();
};

// What a worker opens a scene with ("open"). Build & Refresh carries the running scene's state into the worker
// that replaces it; the viewport camera (zoom / pan) lives in the host and needs no transfer.
struct WorkerOpenState {
    std::string id;
    SceneParameters parameters;
    bool paused = false;
    double speed = 1.0;
};

// Worker'ın canlı tuning raporu ("tuning", Docs/Tuning.md): Applied değerler yazıldı; Invalid dosya geçersiz;
// NeedsBuild yapı değişti, derleme gerekir; Unreadable dosya okunamadı. Source UTF-8 dosya yolu.
enum class WorkerTuningState { Applied, Invalid, NeedsBuild, Unreadable };

struct WorkerTuningStatus {
    std::string source;
    WorkerTuningState state = WorkerTuningState::Applied;
    int line = 0;
    std::string text;
    bool reload = false; // false: önizleme açılırken ilk okuma
};

[[nodiscard]] std::vector<std::byte> EncodeWorkerMessage(const WorkerMessage& message);
[[nodiscard]] WorkerMessage DecodeWorkerMessage(const nlohmann::json& json, const std::string& expectedSession);
[[nodiscard]] nlohmann::json EncodeSceneValue(const SceneValue& value);
[[nodiscard]] SceneValue DecodeSceneValue(const nlohmann::json& json);
[[nodiscard]] nlohmann::json EncodeSceneParameters(const SceneParameters& parameters);
[[nodiscard]] SceneParameters DecodeSceneParameters(const nlohmann::json& json);
[[nodiscard]] nlohmann::json EncodeWorkerOpen(const WorkerOpenState& state);
[[nodiscard]] WorkerOpenState DecodeWorkerOpen(const nlohmann::json& json);
[[nodiscard]] bool ValidPlaybackSpeed(double speed);
[[nodiscard]] nlohmann::json EncodeSceneCatalog(const std::vector<SceneDefinition>& definitions);
[[nodiscard]] std::vector<SceneDefinition> DecodeSceneCatalog(const nlohmann::json& json);
[[nodiscard]] nlohmann::json EncodeWorkerInput(const DeviceInput& input);
[[nodiscard]] DeviceInput DecodeWorkerInput(const nlohmann::json& json);
[[nodiscard]] nlohmann::json EncodeSceneInspection(const SceneInspection& inspection);
[[nodiscard]] SceneInspection DecodeSceneInspection(const nlohmann::json& json);
// The project's audio catalog ("audio"), sent before "catalog"; also in `--audio-report` answers. Effects carry
// maxConcurrent, volume and overflow ("restart-next" | "skip"); music its group ("default" | "menu").
[[nodiscard]] nlohmann::json EncodeAudioCatalog(const AudioCatalog& catalog);
[[nodiscard]] std::vector<AudioUse> DecodeAudioCatalog(const nlohmann::json& json);
[[nodiscard]] nlohmann::json EncodeWorkerTuning(const WorkerTuningStatus& status);
[[nodiscard]] WorkerTuningStatus DecodeWorkerTuning(const nlohmann::json& json);

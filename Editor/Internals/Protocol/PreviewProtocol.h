#pragma once
#include <nlohmann/json.hpp>
#include <cstddef>
#include <cstdint>
#include <string>
#include <vector>
#include "Editor/Api/PreviewRegistry.h"
#include "Input/Input.hpp"

inline constexpr std::uint32_t PreviewProtocolVersion = 8;
inline constexpr std::size_t PreviewMaxInputPoints = 32;
inline constexpr std::size_t PreviewMaxMessageBytes = 1024 * 1024;
inline constexpr std::size_t PreviewMaxQueuedMessages = 256;
inline constexpr double PreviewMinPlaybackSpeed = 0.1;
inline constexpr double PreviewMaxPlaybackSpeed = 4.0;

struct PreviewMessage {
    std::string session;
    std::uint64_t generation = 0;
    std::uint64_t request = 0;
    std::string type;
    nlohmann::json data = nlohmann::json::object();
};

// What a worker opens a preview with ("open"). Build & Refresh carries the running preview's state into the worker
// that replaces it; the viewport camera (zoom / pan) lives in the host and needs no transfer.
struct PreviewOpenState {
    std::string id;
    PreviewParameters parameters;
    bool paused = false;
    double speed = 1.0;
};

// Worker'ın canlı tuning raporu ("tuning", Docs/Tuning.md): Applied değerler yazıldı; Invalid dosya geçersiz;
// NeedsBuild yapı değişti, derleme gerekir; Unreadable dosya okunamadı. Source UTF-8 dosya yolu.
enum class PreviewTuningState { Applied, Invalid, NeedsBuild, Unreadable };

struct PreviewTuningStatus {
    std::string source;
    PreviewTuningState state = PreviewTuningState::Applied;
    int line = 0;
    std::string text;
    bool reload = false; // false: önizleme açılırken ilk okuma
};

[[nodiscard]] std::vector<std::byte> EncodePreviewMessage(const PreviewMessage& message);
[[nodiscard]] PreviewMessage DecodePreviewMessage(const nlohmann::json& json, const std::string& expectedSession);
[[nodiscard]] nlohmann::json EncodePreviewValue(const PreviewValue& value);
[[nodiscard]] PreviewValue DecodePreviewValue(const nlohmann::json& json);
[[nodiscard]] nlohmann::json EncodePreviewParameters(const PreviewParameters& parameters);
[[nodiscard]] PreviewParameters DecodePreviewParameters(const nlohmann::json& json);
[[nodiscard]] nlohmann::json EncodePreviewOpen(const PreviewOpenState& state);
[[nodiscard]] PreviewOpenState DecodePreviewOpen(const nlohmann::json& json);
[[nodiscard]] bool ValidPlaybackSpeed(double speed);
[[nodiscard]] nlohmann::json EncodePreviewCatalog(const std::vector<PreviewDefinition>& definitions);
[[nodiscard]] std::vector<PreviewDefinition> DecodePreviewCatalog(const nlohmann::json& json);
[[nodiscard]] nlohmann::json EncodePreviewInput(const DeviceInput& input);
[[nodiscard]] DeviceInput DecodePreviewInput(const nlohmann::json& json);
[[nodiscard]] nlohmann::json EncodePreviewInspection(const PreviewInspection& inspection);
[[nodiscard]] PreviewInspection DecodePreviewInspection(const nlohmann::json& json);
[[nodiscard]] nlohmann::json EncodePreviewTuning(const PreviewTuningStatus& status);
[[nodiscard]] PreviewTuningStatus DecodePreviewTuning(const nlohmann::json& json);

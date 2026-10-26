#pragma once
#include <nlohmann/json.hpp>
#include <cstddef>
#include <cstdint>
#include <string>
#include <vector>
#include "Editor/Api/PreviewRegistry.h"
#include "Input/DeviceInput.h"

inline constexpr std::uint32_t PreviewProtocolVersion = 1;
inline constexpr std::size_t PreviewMaxMessageBytes = 1024 * 1024;
inline constexpr std::size_t PreviewMaxQueuedMessages = 256;

struct PreviewMessage {
    std::string session;
    std::uint64_t generation = 0;
    std::uint64_t request = 0;
    std::string type;
    nlohmann::json data = nlohmann::json::object();
};

[[nodiscard]] std::vector<std::byte> EncodePreviewMessage(const PreviewMessage& message);
[[nodiscard]] PreviewMessage DecodePreviewMessage(const nlohmann::json& json, const std::string& expectedSession);
[[nodiscard]] nlohmann::json EncodePreviewValue(const PreviewValue& value);
[[nodiscard]] PreviewValue DecodePreviewValue(const nlohmann::json& json);
[[nodiscard]] nlohmann::json EncodePreviewParameters(const PreviewParameters& parameters);
[[nodiscard]] PreviewParameters DecodePreviewParameters(const nlohmann::json& json);
[[nodiscard]] nlohmann::json EncodePreviewCatalog(const std::vector<PreviewDefinition>& definitions);
[[nodiscard]] std::vector<PreviewDefinition> DecodePreviewCatalog(const nlohmann::json& json);
[[nodiscard]] nlohmann::json EncodePreviewInput(const DeviceInput& input);
[[nodiscard]] DeviceInput DecodePreviewInput(const nlohmann::json& json);
[[nodiscard]] nlohmann::json EncodePreviewInspection(const PreviewInspection& inspection);
[[nodiscard]] PreviewInspection DecodePreviewInspection(const nlohmann::json& json);

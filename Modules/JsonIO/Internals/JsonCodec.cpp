#include "Modules/JsonIO/Internals/JsonCodec.h"
#include <nlohmann/json.hpp>
#include <cmath>
#include <type_traits>
#include "Modules/JsonIO/Internals/JsonSax.h"

namespace {
nlohmann::json Convert(const JsonIO::Document& document, std::size_t depth, std::size_t maxDepth) {
    return std::visit(
        [&](const auto& value) -> nlohmann::json {
            using T = std::decay_t<decltype(value)>;
            if constexpr (std::same_as<T, JsonIO::Document::Object> || std::same_as<T, JsonIO::Document::Array>) {
                if (depth >= maxDepth)
                    throw JsonIO::Error(JsonIO::ErrorCode::LimitExceeded, "JSON nesting exceeds the depth limit");
                if constexpr (std::same_as<T, JsonIO::Document::Object>) {
                    auto result = nlohmann::json::object();
                    for (const auto& [key, item] : value)
                        result[key] = Convert(item, depth + 1, maxDepth);
                    return result;
                } else {
                    auto result = nlohmann::json::array();
                    for (const auto& item : value)
                        result.push_back(Convert(item, depth + 1, maxDepth));
                    return result;
                }
            } else {
                if constexpr (std::same_as<T, double>) {
                    if (!std::isfinite(value))
                        throw JsonIO::Error(JsonIO::ErrorCode::NumberOutOfRange, "JSON numbers must be finite");
                }
                return value;
            }
        },
        document.value);
}
void ValidateLimits(JsonIO::Limits limits) {
    if (limits.maxBytes == 0 || limits.maxDepth == 0)
        throw JsonIO::Error(JsonIO::ErrorCode::InvalidOptions, "JSON limits must be positive");
}
} // namespace

JsonIO::Document JsonCodec::Parse(std::string_view text, JsonIO::Limits limits) {
    ValidateLimits(limits);
    if (text.size() > limits.maxBytes)
        throw JsonIO::Error(JsonIO::ErrorCode::LimitExceeded, "JSON input exceeds the byte limit");
    if (const auto position = text.find('\0'); position != std::string_view::npos)
        throw JsonIO::Error(JsonIO::ErrorCode::InvalidJson, "Unescaped NUL in JSON input", position + 1);
    try {
        JsonSax parser(limits.maxDepth);
        if (!nlohmann::json::sax_parse(text.begin(), text.end(), &parser))
            throw JsonIO::Error(JsonIO::ErrorCode::InvalidJson, "JSON parser rejected the input");
        return parser.Take();
    } catch (const nlohmann::json::exception& error) {
        throw JsonIO::Error(JsonIO::ErrorCode::InvalidJson, error.what());
    }
}
std::string JsonCodec::Serialize(const JsonIO::Document& document, const JsonIO::SerializeOptions& options) {
    ValidateLimits(options.limits);
    try {
        auto text =
            Convert(document, 0, options.limits.maxDepth).dump(options.format == JsonIO::Format::Pretty ? 4 : -1);
        if (text.size() > options.limits.maxBytes)
            throw JsonIO::Error(JsonIO::ErrorCode::LimitExceeded, "JSON output exceeds the byte limit");
        return text;
    } catch (const nlohmann::json::exception& error) {
        throw JsonIO::Error(JsonIO::ErrorCode::InvalidJson, error.what());
    }
}

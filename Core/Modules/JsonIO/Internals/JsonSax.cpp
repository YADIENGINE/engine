#include "Modules/JsonIO/Internals/JsonSax.h"
#include <cmath>
#include <limits>
#include <utility>

namespace {
constexpr int BackendNumberOverflow = 406;
} // namespace

JsonSax::JsonSax(std::size_t maxDepth)
    : _maxDepth(maxDepth) {}
bool JsonSax::Add(JsonIO::Document value) {
    if (_frames.empty())
        _root = std::move(value);
    else {
        auto& frame = _frames.back();
        if (auto* object = std::get_if<JsonIO::Document::Object>(&frame.document.value))
            object->emplace(std::move(frame.key), std::move(value));
        else
            frame.document.Append(std::move(value));
    }
    return true;
}
bool JsonSax::Start(JsonIO::Document value) {
    if (_frames.size() >= _maxDepth)
        throw JsonIO::Error(JsonIO::ErrorCode::LimitExceeded, "JSON nesting exceeds the depth limit");
    _frames.push_back({std::move(value), {}});
    return true;
}
bool JsonSax::End() {
    auto value = std::move(_frames.back().document);
    _frames.pop_back();
    return Add(std::move(value));
}
bool JsonSax::null() { return Add(nullptr); }
bool JsonSax::boolean(bool value) { return Add(value); }
bool JsonSax::number_integer(number_integer_t value) { return Add(value); }
bool JsonSax::number_unsigned(number_unsigned_t value) {
    // Canonicalize small positive integers so signed values survive a round trip.
    if (value <= static_cast<std::uint64_t>(std::numeric_limits<std::int64_t>::max()))
        return Add(static_cast<std::int64_t>(value));
    return Add(value);
}
bool JsonSax::number_float(number_float_t value, const string_t& token) {
    if (token.find_first_of(".eE") == std::string::npos || !std::isfinite(value))
        throw JsonIO::Error(JsonIO::ErrorCode::NumberOutOfRange, "JSON number exceeds the supported range");
    return Add(value);
}
bool JsonSax::string(string_t& value) { return Add(std::move(value)); }
bool JsonSax::binary(binary_t&) {
    throw JsonIO::Error(JsonIO::ErrorCode::InvalidJson, "Binary JSON values are not supported");
}
bool JsonSax::start_object(std::size_t) { return Start(JsonIO::Document::Object{}); }
bool JsonSax::key(string_t& value) {
    auto& frame = _frames.back();
    if (frame.document.Members().contains(value))
        throw JsonIO::Error(JsonIO::ErrorCode::InvalidJson, "Duplicate JSON member: " + value);
    frame.key = std::move(value);
    return true;
}
bool JsonSax::end_object() { return End(); }
bool JsonSax::start_array(std::size_t) { return Start(JsonIO::Document::Array{}); }
bool JsonSax::end_array() { return End(); }
bool JsonSax::parse_error(std::size_t position, const std::string&, const nlohmann::detail::exception& error) {
    const auto code =
        error.id == BackendNumberOverflow ? JsonIO::ErrorCode::NumberOutOfRange : JsonIO::ErrorCode::InvalidJson;
    throw JsonIO::Error(code, error.what(), position);
}
JsonIO::Document JsonSax::Take() { return std::move(_root); }

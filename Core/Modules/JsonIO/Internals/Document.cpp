#include "Modules/JsonIO/JsonIO.h"
#include <limits>
#include <utility>

namespace {
template <class T> const T& Require(const JsonIO::Document::Value& value) {
    if (const auto* item = std::get_if<T>(&value)) return *item;
    throw JsonIO::Error(JsonIO::ErrorCode::TypeMismatch, "JSON value has a different type");
}
template <class T> T& Require(JsonIO::Document::Value& value) {
    if (auto* item = std::get_if<T>(&value)) return *item;
    throw JsonIO::Error(JsonIO::ErrorCode::TypeMismatch, "JSON value has a different type");
}
} // namespace

JsonIO::Error::Error(ErrorCode code, const std::string& message, std::size_t byte)
    : std::runtime_error(message)
    , _code(code)
    , _byte(byte) {}
JsonIO::ErrorCode JsonIO::Error::Code() const noexcept { return _code; }
std::size_t JsonIO::Error::Byte() const noexcept { return _byte; }

JsonIO::Document::Document(std::nullptr_t) {}
JsonIO::Document::Document(bool item)
    : value(item) {}
JsonIO::Document::Document(double item)
    : value(item) {}
JsonIO::Document::Document(const char* text)
    : value(std::string(text)) {}
JsonIO::Document::Document(std::string text)
    : value(std::move(text)) {}
JsonIO::Document::Document(std::string_view text)
    : value(std::string(text)) {}
JsonIO::Document::Document(Object object)
    : value(std::move(object)) {}
JsonIO::Document::Document(Array array)
    : value(std::move(array)) {}
JsonIO::Document JsonIO::Document::MakeObject() { return Object{}; }
JsonIO::Document JsonIO::Document::MakeArray() { return Array{}; }
JsonIO::Document& JsonIO::Document::operator[](std::string key) { return Members()[std::move(key)]; }
const JsonIO::Document& JsonIO::Document::At(std::string_view key) const {
    const auto& object = Members();
    const auto found = object.find(key);
    if (found == object.end()) throw Error(ErrorCode::MissingMember, "Missing JSON member: " + std::string(key));
    return found->second;
}
JsonIO::Document& JsonIO::Document::At(std::string_view key) {
    auto& object = Members();
    const auto found = object.find(key);
    if (found == object.end()) throw Error(ErrorCode::MissingMember, "Missing JSON member: " + std::string(key));
    return found->second;
}
void JsonIO::Document::Append(Document item) { Elements().push_back(std::move(item)); }
const JsonIO::Document::Object& JsonIO::Document::Members() const { return Require<Object>(value); }
JsonIO::Document::Object& JsonIO::Document::Members() { return Require<Object>(value); }
const JsonIO::Document::Array& JsonIO::Document::Elements() const { return Require<Array>(value); }
JsonIO::Document::Array& JsonIO::Document::Elements() { return Require<Array>(value); }
std::int64_t JsonIO::Document::AsInt64() const {
    if (const auto* item = std::get_if<std::int64_t>(&value)) return *item;
    if (const auto* item = std::get_if<std::uint64_t>(&value)) {
        if (*item <= static_cast<std::uint64_t>(std::numeric_limits<std::int64_t>::max()))
            return static_cast<std::int64_t>(*item);
        throw Error(ErrorCode::NumberOutOfRange, "JSON integer does not fit int64");
    }
    throw Error(ErrorCode::TypeMismatch, "JSON value is not an integer");
}
std::uint64_t JsonIO::Document::AsUInt64() const {
    if (const auto* item = std::get_if<std::uint64_t>(&value)) return *item;
    if (const auto* item = std::get_if<std::int64_t>(&value)) {
        if (*item >= 0) return static_cast<std::uint64_t>(*item);
        throw Error(ErrorCode::NumberOutOfRange, "Negative JSON integer does not fit uint64");
    }
    throw Error(ErrorCode::TypeMismatch, "JSON value is not an integer");
}
double JsonIO::Document::AsDouble() const { return Require<double>(value); }
const std::string& JsonIO::Document::AsString() const { return Require<std::string>(value); }
bool JsonIO::Document::AsBool() const { return Require<bool>(value); }
bool JsonIO::Document::IsNull() const { return std::holds_alternative<std::nullptr_t>(value); }
bool JsonIO::Document::operator==(const Document& other) const {
    if (value.index() == other.value.index()) return value == other.value;
    const auto* leftSigned = std::get_if<std::int64_t>(&value);
    const auto* rightUnsigned = std::get_if<std::uint64_t>(&other.value);
    if (leftSigned && rightUnsigned)
        return *leftSigned >= 0 && static_cast<std::uint64_t>(*leftSigned) == *rightUnsigned;
    const auto* leftUnsigned = std::get_if<std::uint64_t>(&value);
    const auto* rightSigned = std::get_if<std::int64_t>(&other.value);
    return leftUnsigned && rightSigned && *rightSigned >= 0 &&
           *leftUnsigned == static_cast<std::uint64_t>(*rightSigned);
}

#pragma once
#include "Modules/JsonIO/JsonIO.h"

// Replace this adapter's implementation to change the JSON library.
class JsonCodec final {
public:
    [[nodiscard]] static JsonIO::Document Parse(std::string_view text, JsonIO::Limits limits);
    [[nodiscard]] static std::string Serialize(const JsonIO::Document& document,
                                               const JsonIO::SerializeOptions& options);
};

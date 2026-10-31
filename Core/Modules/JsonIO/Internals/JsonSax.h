#pragma once
#include <nlohmann/json.hpp>
#include "Modules/JsonIO/JsonIO.h"

class JsonSax final : public nlohmann::json_sax<nlohmann::json> {
public:
    explicit JsonSax(std::size_t maxDepth);
    bool null() override;
    bool boolean(bool value) override;
    bool number_integer(number_integer_t value) override;
    bool number_unsigned(number_unsigned_t value) override;
    bool number_float(number_float_t value, const string_t& token) override;
    bool string(string_t& value) override;
    bool binary(binary_t& value) override;
    bool start_object(std::size_t elements) override;
    bool key(string_t& value) override;
    bool end_object() override;
    bool start_array(std::size_t elements) override;
    bool end_array() override;
    bool parse_error(std::size_t position, const std::string& token, const nlohmann::detail::exception& error) override;
    [[nodiscard]] JsonIO::Document Take();

private:
    struct Frame {
        JsonIO::Document document;
        std::string key;
    };
    bool Add(JsonIO::Document value);
    bool Start(JsonIO::Document value);
    bool End();

    std::size_t _maxDepth;
    std::vector<Frame> _frames;
    JsonIO::Document _root;
};

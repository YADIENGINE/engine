#include "Editor/Internals/Protocol/PixelatorProtocol.h"
#include <array>
#include <cmath>
#include <cstdint>
#include <format>
#include <stdexcept>
#include <string>
#include <string_view>
#include <vector>
#include "Editor/Internals/Pixelator/PixelatorPicture.h"

namespace {
constexpr std::string_view Base64Alphabet = "ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz0123456789+/";
constexpr std::size_t MaxImages = 1024;
constexpr std::size_t MaxFrames = 1024;
constexpr std::size_t MaxItems = 4096;

std::string Base64(const std::vector<std::uint8_t>& bytes) {
    std::string text;
    text.reserve((bytes.size() + 2) / 3 * 4);
    for (std::size_t at = 0; at < bytes.size(); at += 3) {
        const std::uint32_t chunk = (static_cast<std::uint32_t>(bytes[at]) << 16) |
                                    (at + 1 < bytes.size() ? static_cast<std::uint32_t>(bytes[at + 1]) << 8 : 0u) |
                                    (at + 2 < bytes.size() ? static_cast<std::uint32_t>(bytes[at + 2]) : 0u);
        text += Base64Alphabet[(chunk >> 18) & 63];
        text += Base64Alphabet[(chunk >> 12) & 63];
        text += at + 1 < bytes.size() ? Base64Alphabet[(chunk >> 6) & 63] : '=';
        text += at + 2 < bytes.size() ? Base64Alphabet[chunk & 63] : '=';
    }
    return text;
}

std::vector<std::uint8_t> FromBase64(const std::string& text) {
    if (text.size() % 4 != 0) throw std::runtime_error("Invalid base64 length");
    std::array<int, 256> values{};
    values.fill(-1);
    for (std::size_t index = 0; index < Base64Alphabet.size(); ++index)
        values[static_cast<unsigned char>(Base64Alphabet[index])] = static_cast<int>(index);
    std::vector<std::uint8_t> bytes;
    bytes.reserve(text.size() / 4 * 3);
    for (std::size_t at = 0; at < text.size(); at += 4) {
        std::uint32_t chunk = 0;
        int padding = 0;
        for (std::size_t offset = 0; offset < 4; ++offset) {
            const char character = text[at + offset];
            int value = 0;
            if (character == '=' && at + 4 == text.size() && offset >= 2) {
                ++padding;
            } else {
                value = values[static_cast<unsigned char>(character)];
                if (value < 0 || padding > 0) throw std::runtime_error("Invalid base64 text");
            }
            chunk = (chunk << 6) | static_cast<std::uint32_t>(value);
        }
        bytes.push_back(static_cast<std::uint8_t>(chunk >> 16));
        if (padding < 2) bytes.push_back(static_cast<std::uint8_t>(chunk >> 8));
        if (padding < 1) bytes.push_back(static_cast<std::uint8_t>(chunk));
    }
    return bytes;
}

std::uint64_t Index(const nlohmann::json& json, std::uint64_t limit, const char* what) {
    if (!json.is_number_unsigned() || json.get<std::uint64_t>() >= limit)
        throw std::runtime_error(std::string("Invalid pixel graphic ") + what);
    return json.get<std::uint64_t>();
}

Color4 ColorOf(const std::string& text) {
    if (text.size() != 9 || text[0] != '#') throw std::runtime_error("Invalid pixel palette colour");
    const auto channel = [&](std::size_t at) {
        return static_cast<std::uint8_t>(std::stoul(text.substr(at, 2), nullptr, 16));
    };
    return {channel(1), channel(3), channel(5), channel(7)};
}
} // namespace

std::string PixelatorHex(Color4 color) {
    return std::format("#{:02x}{:02x}{:02x}{}", color.r, color.g, color.b,
                       color.a == 255 ? std::string() : std::format("{:02x}", color.a));
}

nlohmann::json EncodePixelatorAsset(const PixelatorAsset& asset) {
    nlohmann::json json{{"id", asset.id},
                        {"item", asset.item},
                        {"window", asset.window},
                        {"looped", asset.looped},
                        {"capped", asset.capped}};
    if (!asset.error.empty()) json["error"] = asset.error;
    auto items = nlohmann::json::array();
    for (const auto& item : asset.items)
        items.push_back({{"name", item.name}, {"id", item.id}});
    json["items"] = std::move(items);
    auto images = nlohmann::json::array();
    for (const auto& image : asset.images)
        images.push_back({{"size", {image.width, image.height}},
                          {"pivot", {image.pivot.x, image.pivot.y}},
                          {"inks", Base64(image.inks)},
                          {"parts", Base64(image.parts)}});
    json["images"] = std::move(images);
    auto palettes = nlohmann::json::array();
    for (const auto& palette : asset.palettes) {
        auto colors = nlohmann::json::array();
        for (const Color4 color : palette)
            colors.push_back(std::format("#{:02x}{:02x}{:02x}{:02x}", color.r, color.g, color.b, color.a));
        palettes.push_back(std::move(colors));
    }
    json["palettes"] = std::move(palettes);
    auto frames = nlohmann::json::array();
    for (const auto& frame : asset.frames)
        frames.push_back({{"image", frame.image}, {"palette", frame.palette}, {"seconds", frame.seconds}});
    json["frames"] = std::move(frames);
    return json;
}

PixelatorAsset DecodePixelatorAsset(const nlohmann::json& json) {
    PixelatorAsset asset;
    asset.id = json.at("id").get<std::string>();
    if (asset.id.empty()) throw std::runtime_error("Pixel graphic without an id");
    asset.error = json.value("error", std::string());
    asset.window = json.at("window").get<float>();
    asset.looped = json.at("looped").get<bool>();
    asset.capped = json.at("capped").get<bool>();
    if (!std::isfinite(asset.window) || asset.window < 0.0f) throw std::runtime_error("Invalid pixel sampling window");
    const auto& items = json.at("items");
    if (!items.is_array() || items.size() > MaxItems) throw std::runtime_error("Invalid pixel graphic items");
    for (const auto& item : items)
        asset.items.push_back({item.at("name").get<std::string>(), item.at("id").get<std::string>()});
    asset.item = asset.items.empty() ? 0 : static_cast<std::size_t>(Index(json.at("item"), asset.items.size(), "item"));
    const auto& images = json.at("images");
    if (!images.is_array() || images.size() > MaxImages) throw std::runtime_error("Invalid pixel graphic sprites");
    for (const auto& entry : images) {
        PixelatorImage image;
        const auto size = entry.at("size").get<std::vector<int>>();
        const auto pivot = entry.at("pivot").get<std::vector<int>>();
        if (size.size() != 2 || pivot.size() != 2 || size[0] < 0 || size[1] < 0 || size[0] > PixelatorMaxSide ||
            size[1] > PixelatorMaxSide)
            throw std::runtime_error("Invalid pixel sprite size");
        image.width = size[0];
        image.height = size[1];
        image.pivot = {pivot[0], pivot[1]};
        image.inks = FromBase64(entry.at("inks").get<std::string>());
        image.parts = FromBase64(entry.at("parts").get<std::string>());
        const auto pixels = static_cast<std::size_t>(image.width) * static_cast<std::size_t>(image.height);
        if (image.inks.size() != pixels || image.parts.size() != pixels)
            throw std::runtime_error("Pixel sprite data does not match its size");
        asset.images.push_back(std::move(image));
    }
    const auto& palettes = json.at("palettes");
    if (!palettes.is_array() || palettes.size() > MaxImages) throw std::runtime_error("Invalid pixel palettes");
    for (const auto& entry : palettes) {
        if (!entry.is_array() || entry.size() > static_cast<std::size_t>(MaxInks))
            throw std::runtime_error("Invalid pixel palette");
        std::vector<Color4> colors;
        for (const auto& color : entry)
            colors.push_back(ColorOf(color.get<std::string>()));
        asset.palettes.push_back(std::move(colors));
    }
    const auto& frames = json.at("frames");
    if (!frames.is_array() || frames.size() > MaxFrames) throw std::runtime_error("Invalid pixel frames");
    for (const auto& entry : frames) {
        PixelatorFrame frame;
        frame.image = static_cast<std::size_t>(Index(entry.at("image"), asset.images.size(), "frame sprite"));
        frame.palette = static_cast<std::size_t>(Index(entry.at("palette"), asset.palettes.size(), "frame palette"));
        frame.seconds = entry.at("seconds").get<float>();
        if (!std::isfinite(frame.seconds) || frame.seconds < 0.0f) throw std::runtime_error("Invalid frame duration");
        asset.frames.push_back(frame);
    }
    return asset;
}

nlohmann::json PixelatorSummary(const PixelatorAsset& asset, std::size_t frame) {
    nlohmann::json json{{"asset", asset.id},
                        {"frames", asset.frames.size()},
                        {"window", asset.window},
                        {"looped", asset.looped},
                        {"capped", asset.capped}};
    if (!asset.items.empty()) {
        json["item"] = asset.items[asset.item].name;
        auto names = nlohmann::json::array();
        for (const auto& item : asset.items)
            names.push_back(item.name);
        json["items"] = std::move(names);
    }
    if (frame >= asset.frames.size()) return json;
    const auto& shown = asset.frames[frame];
    const auto& image = asset.images[shown.image];
    const auto& palette = asset.palettes[shown.palette];
    json["frame"] = frame;
    json["seconds"] = shown.seconds;
    json["size"] = {image.width, image.height};
    json["pivot"] = {image.pivot.x, image.pivot.y};
    auto inks = nlohmann::json::array();
    for (const auto& count : PixelatorInkCounts(image)) {
        const Color4 color = count.index < static_cast<int>(palette.size())
                                 ? palette[static_cast<std::size_t>(count.index)]
                                 : Color4{0, 0, 0, 0};
        inks.push_back({{"ink", count.index}, {"color", PixelatorHex(color)}, {"pixels", count.pixels}});
    }
    json["inks"] = std::move(inks);
    auto parts = nlohmann::json::array();
    for (const auto& count : PixelatorPartCounts(image))
        parts.push_back({{"part", count.index}, {"pixels", count.pixels}});
    json["parts"] = std::move(parts);
    return json;
}

const char* PixelatorViewName(PixelatorViewMode view) {
    switch (view) {
    case PixelatorViewMode::Color: return "color";
    case PixelatorViewMode::Inks : return "inks";
    case PixelatorViewMode::Parts: return "parts";
    }
    return "color";
}

PixelatorRequest DecodePixelatorRequest(const nlohmann::json& arguments) {
    PixelatorRequest request;
    auto& picture = request.picture;
    const auto whole = [&](const char* key, int minimum, int maximum) {
        const auto& value = arguments.at(key);
        if (!value.is_number_integer() || value.get<long long>() < minimum || value.get<long long>() > maximum)
            throw std::runtime_error(std::format("\"{}\" must be a whole number from {} to {}", key, minimum, maximum));
        return value.get<int>();
    };
    const auto flag = [&](const char* key) {
        const auto& value = arguments.at(key);
        if (!value.is_boolean()) throw std::runtime_error(std::format("\"{}\" must be true or false", key));
        return value.get<bool>();
    };
    const auto word = [&](const char* key) {
        const auto& value = arguments.at(key);
        if (!value.is_string()) throw std::runtime_error(std::format("\"{}\" must be text", key));
        return value.get<std::string>();
    };
    if (arguments.contains("view")) {
        const auto view = word("view");
        if (view == "color")
            picture.view = PixelatorViewMode::Color;
        else if (view == "inks")
            picture.view = PixelatorViewMode::Inks;
        else if (view == "parts")
            picture.view = PixelatorViewMode::Parts;
        else
            throw std::runtime_error("\"view\" is color, inks or parts");
    }
    if (arguments.contains("zoom")) picture.zoom = whole("zoom", 1, 64);
    if (arguments.contains("frame")) request.frame = static_cast<std::size_t>(whole("frame", 0, 1 << 20));
    if (arguments.contains("grid")) picture.grid = flag("grid");
    if (arguments.contains("tiles")) picture.tiles = whole("tiles", 0, 256);
    if (arguments.contains("pivot")) picture.pivot = flag("pivot");
    if (arguments.contains("ink")) picture.focus = static_cast<Ink>(whole("ink", 1, MaxInks - 1));
    if (arguments.contains("background")) {
        const auto background = word("background");
        if (background == "checker")
            picture.background = PixelatorBackground::Checker;
        else if (background == "dark")
            picture.background = PixelatorBackground::Dark;
        else if (background == "light")
            picture.background = PixelatorBackground::Light;
        else
            throw std::runtime_error("\"background\" is checker, dark or light");
    }
    return request;
}

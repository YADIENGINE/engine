#include "Editor/Worker/PixelLintReport.h"
#include <nlohmann/json.hpp>
#include <fstream>
#include <stdexcept>
#include <string>
#include <vector>
#include "Editor/Internals/Protocol/PixelatorProtocol.h"
#include "Editor/Internals/Session/GraphicAssetCatalog.h"
#include "Editor/Worker/PixelatorReport.h"

namespace {
constexpr int ReportVersion = 1;

std::filesystem::path PathOf(const std::string& value) {
    return std::u8string(reinterpret_cast<const char8_t*>(value.data()), value.size());
}

// "Graphics/X", "asset:Graphics/X" and "asset:Graphics/X#Item" all name catalog ids.
std::string CatalogId(std::string id) { return id.starts_with("asset:") ? id : "asset:" + id; }
} // namespace

void WritePixelLintReport(const std::filesystem::path& request) {
    std::ifstream input(request, std::ios::binary);
    if (!input) throw std::runtime_error("Cannot read the pixel lint request: " + request.string());
    const auto json = nlohmann::json::parse(input);
    const auto out = PathOf(json.at("out").get<std::string>());
    const auto wanted = json.value("ids", std::vector<std::string>{});
    const auto graphics = RegisteredGraphicThumbnails();
    std::vector<std::string> ids;
    auto unknown = nlohmann::json::array();
    const auto pixelGraphic = [](const GraphicThumbnail& graphic) { return static_cast<bool>(graphic.asset->pixels); };
    if (wanted.empty()) {
        for (const auto& graphic : graphics)
            if (pixelGraphic(graphic)) ids.push_back(graphic.id);
    }
    for (const auto& text : wanted) {
        const auto id = CatalogId(text);
        bool found = false;
        for (const auto& graphic : graphics)
            if (graphic.id == id || (id.find('#') == std::string::npos && graphic.id.starts_with(id + "#"))) {
                ids.push_back(graphic.id);
                found = true;
            }
        if (!found) unknown.push_back(text);
    }
    auto reports = nlohmann::json::array();
    for (const auto& id : ids)
        reports.push_back(PixelatorLintReport(PixelatorReport(id)));
    nlohmann::json answer{{"version", ReportVersion}, {"reports", std::move(reports)}, {"unknown", std::move(unknown)}};
    std::ofstream output(out, std::ios::binary);
    if (!output) throw std::runtime_error("Cannot write the pixel lint answer");
    output << answer.dump(2);
}

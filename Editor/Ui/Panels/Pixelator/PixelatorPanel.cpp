#include "Editor/Ui/Panels/Pixelator/PixelatorPanel.h"
#include <extras/IconsFontAwesome6.h>
#include <imgui.h>
#include <algorithm>
#include <array>
#include <cmath>
#include <format>
#include <string>
#include <vector>
#include "Editor/Internals/Protocol/PixelatorProtocol.h"
#include "ToolUi/ToolTheme.h"
#include "ToolUi/ToolWidgets.h"

namespace {
constexpr int MaxGameScale = 8; // Largest game-size preview factor offered
constexpr std::size_t SpriteSlot = 0;  // The view: colours per mode, focus dimmed
constexpr std::size_t ColorSlot = 1;   // The game-size preview: plain colours
constexpr std::size_t CheckerSlot = 2; // A 2x2 checker, repeated
constexpr float SideWidth = 210.0f;
constexpr float CompactWidth = 760.0f; // Toolbar room below which it folds its rarer controls
constexpr float SwatchSize = 14.0f;
constexpr float InsetMaxHeight = 220.0f;
constexpr float InsetMaxShare = 0.4f; // Of the side strip's height; the ink list keeps the rest
constexpr float PivotArm = 7.0f;
constexpr float FrameBarHeight = 10.0f;
constexpr std::array TileChoices{0, 8, 16, 32};
constexpr std::array BackgroundNames{"Checker", "Dark", "Light"};
constexpr std::array ViewNames{"Color", "Inks", "Parts"};

ImU32 U32(Color4 color) { return IM_COL32(color.r, color.g, color.b, color.a); }

ImVec2 Add(ImVec2 a, ImVec2 b) { return {a.x + b.x, a.y + b.y}; }

std::string Milliseconds(float seconds) { return std::format("{:.0f} ms", seconds * 1000.0f); }

// The image a frame shows, or an empty one.
const PixelatorImage& ImageOf(const PixelatorAsset& asset, std::size_t frame) {
    static const PixelatorImage Empty;
    if (frame >= asset.frames.size()) return Empty;
    return asset.images[asset.frames[frame].image];
}
const std::vector<Color4>& PaletteOf(const PixelatorAsset& asset, std::size_t frame) {
    static const std::vector<Color4> Empty;
    if (frame >= asset.frames.size()) return Empty;
    return asset.palettes[asset.frames[frame].palette];
}
} // namespace

void PixelatorPanel::Adopt(const std::shared_ptr<const PixelatorAsset>& asset) {
    if (asset == _asset) return;
    const auto base = [](const std::string& id) { return id.substr(0, id.find('#')); };
    const bool other = !_asset || !asset || base(_asset->id) != base(asset->id);
    _asset = asset;
    _playback.Reset(_asset ? std::span<const PixelatorFrame>(_asset->frames) : std::span<const PixelatorFrame>{});
    _spriteShown = {};
    _colorShown = {};
    if (other) {
        _isolated.reset();
        _view = {}; // Fitted again for another graphic; the same graphic's other item keeps the zoom and place
    }
}

void PixelatorPanel::Upload(const EditorUiActions& actions, const PixelatorImage& image,
                            const std::vector<Color4>& palette) {
    if (!actions.pixelatorTexture || image.Empty()) return;
    const std::optional<Ink> focus = _hoverInk ? _hoverInk : _isolated;
    const Shown sprite{_asset.get(), _playback.Frame(), _mode, focus};
    if (sprite != _spriteShown || !_spriteTexture) {
        const auto colors = PixelatorColors(image, palette, _mode, focus);
        _spriteTexture = actions.pixelatorTexture(SpriteSlot, image.width, image.height, colors);
        _spriteShown = sprite;
    }
    const Shown plain{_asset.get(), _playback.Frame(), PixelatorViewMode::Color, std::nullopt};
    if (plain != _colorShown || !_colorTexture) {
        const auto colors = PixelatorColors(image, palette, PixelatorViewMode::Color);
        _colorTexture = actions.pixelatorTexture(ColorSlot, image.width, image.height, colors);
        _colorShown = plain;
    }
    if (!_checkerTexture) {
        const std::array checker{PixelatorCheckerLight, PixelatorCheckerDark, PixelatorCheckerDark,
                                 PixelatorCheckerLight};
        _checkerTexture = actions.pixelatorTexture(CheckerSlot, 2, 2, checker);
    }
}

void PixelatorPanel::ApplyKeys(const EditorPixelatorKeys& keys, const PixelatorImage& image) {
    const Vec2 size{image.width, image.height};
    const Float2 area{_canvasSize.x, _canvasSize.y};
    if (keys.fit) _view.Fit(area, size);
    if (keys.actual) _view.Actual(area, size);
    if (keys.zoom != 0) _view.ZoomAt(area * 0.5f, keys.zoom);
    if (keys.grid) _grid = !_grid;
    if (keys.frame != 0 && _asset) _playback.Step(_asset->frames, keys.frame);
}

void PixelatorPanel::Toolbar(const EditorSnapshot& snapshot, const EditorUiActions& actions,
                             const PixelatorImage& image) {
    const float scale = ToolUiScale();
    if (!_asset->items.empty()) {
        const auto& current = _asset->items[_asset->item];
        ImGui::SetNextItemWidth(std::max(ImGui::CalcTextSize(current.name.c_str()).x + 40.0f * scale, 80.0f * scale));
        ImGui::BeginDisabled(snapshot.busy);
        if (ToolBeginCombo("##pixelator-item", current.name.c_str())) {
            for (std::size_t index = 0; index < _asset->items.size(); ++index) {
                const auto& item = _asset->items[index];
                if (ImGui::Selectable(item.name.c_str(), index == _asset->item) && index != _asset->item &&
                    actions.select && actions.command) {
                    actions.select(item.id);
                    actions.command(EditorCommand::Open);
                }
            }
            ImGui::EndCombo();
        }
        ImGui::EndDisabled();
        ToolTooltip("The item shown; it opens in the Viewport too.");
        ContinueToolToolbar("Color", true);
    }
    // Narrow: the view is a list and tiles and background sit behind one button, so the canvas keeps its room.
    const bool compact = ImGui::GetContentRegionAvail().x < CompactWidth * scale;
    const auto tilesName = [](int tiles) {
        return tiles == 0 ? std::string("No tiles") : std::format("Tiles {}", tiles);
    };
    const auto tilesCombo = [&] {
        ImGui::SetNextItemWidth(90.0f * scale);
        if (ToolBeginCombo("##pixelator-tiles", tilesName(_tiles).c_str())) {
            for (const int choice : TileChoices)
                if (ImGui::Selectable(tilesName(choice).c_str(), choice == _tiles)) _tiles = choice;
            ImGui::EndCombo();
        }
        ToolTooltip("A tile grid every 8, 16 or 32 pixels");
    };
    const auto backgroundCombo = [&] {
        ImGui::SetNextItemWidth(90.0f * scale);
        if (ToolBeginCombo("##pixelator-background", BackgroundNames[static_cast<std::size_t>(_background)])) {
            for (std::size_t index = 0; index < BackgroundNames.size(); ++index)
                if (ImGui::Selectable(BackgroundNames[index], index == static_cast<std::size_t>(_background)))
                    _background = static_cast<PixelatorBackground>(index);
            ImGui::EndCombo();
        }
        ToolTooltip("Behind the sprite");
    };
    if (compact) {
        ImGui::SetNextItemWidth(80.0f * scale);
        if (ToolBeginCombo("##pixelator-view", ViewNames[static_cast<std::size_t>(_mode)])) {
            for (std::size_t index = 0; index < ViewNames.size(); ++index)
                if (ImGui::Selectable(ViewNames[index], static_cast<std::size_t>(_mode) == index))
                    _mode = static_cast<PixelatorViewMode>(index);
            ImGui::EndCombo();
        }
        ToolTooltip("Color: the palette's colours; Inks / Parts: a distinct colour per ink or part");
    } else {
        for (std::size_t index = 0; index < ViewNames.size(); ++index) {
            if (index > 0) ImGui::SameLine(0, 0);
            if (ToolSegment(ViewNames[index], static_cast<std::size_t>(_mode) == index))
                _mode = static_cast<PixelatorViewMode>(index);
            ToolTooltip(index == 0   ? "The palette's colours"
                        : index == 1 ? "A distinct colour per ink"
                                     : "A distinct colour per part (PixelCanvas::Part)");
        }
    }
    ContinueToolToolbar(ImGui::GetFrameHeight() * (compact ? 3.0f : 2.0f), true);
    if (ToolIconButton("grid", ICON_FA_TABLE_CELLS, "Pixel grid (G); shown from 4x zoom", _grid)) _grid = !_grid;
    ImGui::SameLine();
    if (ToolIconButton("pivot", ICON_FA_CROSSHAIRS, "Pivot: the point the sprite is drawn at", _pivot))
        _pivot = !_pivot;
    if (compact) {
        ImGui::SameLine();
        if (ToolIconButton("more", ICON_FA_ELLIPSIS, "Tile grid and background")) ImGui::OpenPopup("pixelator-more");
        if (ImGui::BeginPopup("pixelator-more")) {
            tilesCombo();
            backgroundCombo();
            ImGui::EndPopup();
        }
    } else {
        ContinueToolToolbar(90.0f * scale, true);
        tilesCombo();
        ContinueToolToolbar(90.0f * scale);
        backgroundCombo();
    }
    const Vec2 size{image.width, image.height};
    const Float2 area{_canvasSize.x, _canvasSize.y};
    const auto zoom = std::format("{}x", _view.Zoom());
    const float zoomWidth = ImGui::GetFrameHeight() * 2 + ImGui::CalcTextSize("64x").x + ImGui::CalcTextSize("Fit").x +
                            ImGui::CalcTextSize("1:1").x + 70.0f * scale;
    ContinueToolToolbar(zoomWidth, true);
    if (ToolIconButton("zoom-out", ICON_FA_MAGNIFYING_GLASS_MINUS, "Zoom out (-)")) _view.ZoomAt(area * 0.5f, -1);
    ImGui::SameLine();
    ImGui::AlignTextToFramePadding();
    ImGui::PushFont(ToolFont(ToolFontRole::Technical), 0);
    ImGui::TextUnformatted(zoom.c_str());
    ImGui::PopFont();
    ImGui::SameLine();
    if (ToolIconButton("zoom-in", ICON_FA_MAGNIFYING_GLASS_PLUS, "Zoom in (+)")) _view.ZoomAt(area * 0.5f, 1);
    ImGui::SameLine();
    if (ToolButton("Fit")) _view.Fit(area, size);
    ToolTooltip("The largest whole zoom that shows the whole sprite (F)");
    ImGui::SameLine();
    if (ToolButton("1:1")) _view.Actual(area, size);
    ToolTooltip("One screen pixel per sprite pixel (1)");
}

void PixelatorPanel::Canvas(const PixelatorImage& image, const std::vector<Color4>& palette, ImVec2 size) {
    (void)palette;
    const float scale = ToolUiScale();
    const ImVec2 start = ImGui::GetCursorScreenPos();
    _canvasSize = size;
    const Vec2 sprite{image.width, image.height};
    _view.Keep({size.x, size.y}, sprite);
    ImGui::InvisibleButton("##pixelator-canvas", size,
                           ImGuiButtonFlags_MouseButtonLeft | ImGuiButtonFlags_MouseButtonMiddle);
    const bool hovered = ImGui::IsItemHovered();
    const auto& io = ImGui::GetIO();
    const Float2 mouse{io.MousePos.x - start.x, io.MousePos.y - start.y};
    if (hovered && io.MouseWheel != 0.0f) _view.ZoomAt(mouse, io.MouseWheel > 0.0f ? 1 : -1);
    const bool panning =
        ImGui::IsItemActive() && (ImGui::IsMouseDown(ImGuiMouseButton_Middle) ||
                                  (ImGui::IsKeyDown(ImGuiKey_Space) && ImGui::IsMouseDown(ImGuiMouseButton_Left)));
    if (panning) {
        _view.Pan({io.MouseDelta.x, io.MouseDelta.y});
        ImGui::SetMouseCursor(ImGuiMouseCursor_ResizeAll);
    } else if (hovered && ImGui::IsKeyDown(ImGuiKey_Space)) {
        ImGui::SetMouseCursor(ImGuiMouseCursor_Hand);
    }
    _hoverPixel = hovered ? _view.PixelAt(mouse, sprite) : std::nullopt;

    auto& draw = *ImGui::GetWindowDrawList();
    const ImVec2 end = Add(start, size);
    draw.PushClipRect(start, end, true);
    draw.AddRectFilled(start, end, ImGui::GetColorU32(ToolColors().content));
    const float zoom = static_cast<float>(_view.Zoom());
    const ImVec2 origin = Add(start, {_view.Origin().x, _view.Origin().y});
    const ImVec2 extent{static_cast<float>(image.width) * zoom, static_cast<float>(image.height) * zoom};
    const ImVec2 spriteEnd = Add(origin, extent);
    if (_background == PixelatorBackground::Checker && _checkerTexture) {
        const float repeat = static_cast<float>(PixelatorCheckerSize(_view.Zoom()) * 2);
        draw.AddImage(ImTextureID(_checkerTexture), origin, spriteEnd, {0, 0}, {extent.x / repeat, extent.y / repeat});
    } else {
        draw.AddRectFilled(origin, spriteEnd, U32(PixelatorBackgroundColor(_background)));
    }
    if (_spriteTexture) draw.AddImage(ImTextureID(_spriteTexture), origin, spriteEnd);
    // Only the lines inside the visible part are drawn: a 640-pixel sprite has hundreds.
    const auto visible = [&](int count, float from, float clipLow, float clipHigh) {
        const int first = std::max(1, static_cast<int>(std::floor((clipLow - from) / zoom)));
        const int last = std::min(count - 1, static_cast<int>(std::ceil((clipHigh - from) / zoom)));
        return std::pair{first, last};
    };
    const auto [firstColumn, lastColumn] = visible(image.width, origin.x, start.x, end.x);
    const auto [firstRow, lastRow] = visible(image.height, origin.y, start.y, end.y);
    const float top = std::max(origin.y, start.y);
    const float bottom = std::min(spriteEnd.y, end.y);
    const float left = std::max(origin.x, start.x);
    const float right = std::min(spriteEnd.x, end.x);
    if (_grid && _view.GridVisible()) {
        const ImU32 color = U32(PixelatorGridColor);
        for (int x = firstColumn; x <= lastColumn; ++x) {
            const float at = origin.x + static_cast<float>(x) * zoom;
            draw.AddRectFilled({at, top}, {at + 1.0f, bottom}, color);
        }
        for (int y = firstRow; y <= lastRow; ++y) {
            const float at = origin.y + static_cast<float>(y) * zoom;
            draw.AddRectFilled({left, at}, {right, at + 1.0f}, color);
        }
    }
    if (_tiles > 0) {
        const ImU32 color = U32(PixelatorTileColor);
        const float width = zoom >= 8.0f ? 2.0f : 1.0f;
        for (const int x : PixelatorTileLines(image.width, _tiles)) {
            const float at = origin.x + static_cast<float>(x) * zoom;
            if (at >= start.x - 2 && at <= end.x + 2)
                draw.AddRectFilled({at - width + 1, top}, {at + 1.0f, bottom}, color);
        }
        for (const int y : PixelatorTileLines(image.height, _tiles)) {
            const float at = origin.y + static_cast<float>(y) * zoom;
            if (at >= start.y - 2 && at <= end.y + 2)
                draw.AddRectFilled({left, at - width + 1}, {right, at + 1.0f}, color);
        }
    }
    if (_pivot) {
        const ImVec2 mark{origin.x + static_cast<float>(image.pivot.x) * zoom,
                          origin.y + static_cast<float>(image.pivot.y) * zoom};
        const float arm = PivotArm * scale;
        const ImU32 color = U32(PixelatorPivotColor);
        draw.AddRectFilled({mark.x - arm, mark.y - 1.0f}, {mark.x + arm, mark.y + 1.0f}, color);
        draw.AddRectFilled({mark.x - 1.0f, mark.y - arm}, {mark.x + 1.0f, mark.y + arm}, color);
    }
    if (_hoverPixel) {
        const ImVec2 at{origin.x + static_cast<float>(_hoverPixel->x) * zoom,
                        origin.y + static_cast<float>(_hoverPixel->y) * zoom};
        draw.AddRect({at.x - 1.0f, at.y - 1.0f}, {at.x + zoom + 1.0f, at.y + zoom + 1.0f}, IM_COL32(0, 0, 0, 200));
        draw.AddRect(at, {at.x + zoom, at.y + zoom}, IM_COL32(255, 255, 255, 230));
    }
    draw.PopClipRect();
}

float PixelatorPanel::InsetHeight(const PixelatorImage& image, float sideHeight) const {
    const float scale = ToolUiScale();
    return std::min({static_cast<float>(image.height * _gameScale) + ToolMetrics::Gap * 2 * scale,
                     InsetMaxHeight * scale, sideHeight * InsetMaxShare});
}

void PixelatorPanel::Inset(const PixelatorImage& image, float width, float height) {
    ImGui::TextColored(ToolColors().secondary, "In game");
    ImGui::SameLine();
    const int largest = std::max(3, _projectArtPixel);
    for (int factor = 1; factor <= largest; ++factor) {
        ImGui::SameLine(0, factor == 1 ? ImGui::GetStyle().ItemSpacing.x : 0);
        if (ToolSegment(std::format("{}x###game{}", factor, factor).c_str(), _gameScale == factor)) {
            _gameScale = factor;
            _gameScaleChosen = true;
        }
    }
    ToolTooltip("The sprite at the size it has in the game: one sprite pixel per this many screen pixels. A pixel "
                "project starts at its art pixel (a 720-line canvas shown 1:1). A sprite larger than the box shows its "
                "middle.");
    const float factor = static_cast<float>(_gameScale);
    const ImVec2 sprite{static_cast<float>(image.width) * factor, static_cast<float>(image.height) * factor};
    const ImVec2 start = ImGui::GetCursorScreenPos();
    ImGui::Dummy({width, height});
    const ImVec2 end = Add(start, {width, height});
    auto& draw = *ImGui::GetWindowDrawList();
    draw.PushClipRect(start, end, true);
    if (_background == PixelatorBackground::Checker && _checkerTexture) {
        const float repeat = static_cast<float>(PixelatorCheckerCell * 2);
        draw.AddImage(ImTextureID(_checkerTexture), start, end, {0, 0}, {width / repeat, height / repeat});
    } else {
        draw.AddRectFilled(start, end, U32(PixelatorBackgroundColor(_background)));
    }
    const ImVec2 at{std::round(start.x + (width - sprite.x) * 0.5f), std::round(start.y + (height - sprite.y) * 0.5f)};
    if (_colorTexture) draw.AddImage(ImTextureID(_colorTexture), at, Add(at, sprite));
    draw.PopClipRect();
}

void PixelatorPanel::Side(const PixelatorImage& image, const std::vector<Color4>& palette, ImVec2 size) {
    const float scale = ToolUiScale();
    _hoverInk.reset();
    if (BeginToolPanel("pixelator-side", size, ToolSurface::Utility, 0, ImGuiWindowFlags_NoScrollbar,
                       ToolMetrics::Gap)) {
        const float width = ImGui::GetContentRegionAvail().x;
        const float insetHeight = InsetHeight(image, size.y) + ImGui::GetFrameHeightWithSpacing() * 2;
        const auto counts = PixelatorInkCounts(image);
        ImGui::TextColored(ToolColors().secondary, "Inks  %zu", counts.size());
        if (_isolated) {
            ImGui::SameLine();
            if (ToolButton("Show all")) _isolated.reset();
            ToolTooltip("Stop isolating an ink (Esc)");
        }
        if (ImGui::BeginChild("pixelator-inks", {0, std::max(ImGui::GetFrameHeight() * 3,
                                                             size.y - insetHeight - ImGui::GetFrameHeight() * 2)})) {
            auto& draw = *ImGui::GetWindowDrawList();
            const float rowWidth = ImGui::GetContentRegionAvail().x; // Inside the list's scroll bar
            for (const auto& count : counts) {
                const auto ink = static_cast<Ink>(count.index);
                const Color4 color = _mode == PixelatorViewMode::Inks ? PixelatorIndexColor(ink)
                                     : ink < palette.size()           ? palette[ink]
                                                                      : Color4{0, 0, 0, 0};
                ImGui::PushID(count.index);
                const auto label = std::format("##ink{}", count.index);
                const bool selected = _isolated && *_isolated == ink;
                const ImVec2 row = ImGui::GetCursorScreenPos();
                if (ImGui::Selectable(label.c_str(), selected, 0, {0, SwatchSize * scale + 2.0f}))
                    _isolated = selected ? std::nullopt : std::optional<Ink>(ink);
                if (ImGui::IsItemHovered()) {
                    _hoverInk = ink;
                    const Color4 actual = ink < palette.size() ? palette[ink] : Color4{0, 0, 0, 0};
                    ToolTooltip(std::format("Ink {}: {}, {} pixels\nHover shows its pixels; click isolates it.", ink,
                                            PixelatorHex(actual), count.pixels)
                                    .c_str());
                }
                const ImVec2 swatch{row.x + 2.0f, row.y + 1.0f};
                const float side = SwatchSize * scale;
                draw.AddRectFilled(swatch, Add(swatch, {side, side}), U32(color), 2.0f * scale);
                draw.AddRect(swatch, Add(swatch, {side, side}), ImGui::GetColorU32(ToolColors().separator),
                             2.0f * scale);
                const auto text = std::format("{:>3}  {}", count.index, PixelatorHex(color));
                draw.AddText({swatch.x + side + ToolMetrics::Gap * scale, row.y + 1.0f},
                             ImGui::GetColorU32(ToolColors().text), text.c_str());
                const auto pixels = std::to_string(count.pixels);
                draw.AddText({row.x + rowWidth - ImGui::CalcTextSize(pixels.c_str()).x - ToolMetrics::Space * scale,
                              row.y + 1.0f},
                             ImGui::GetColorU32(ToolColors().secondary), pixels.c_str());
                ImGui::PopID();
            }
            if (_mode == PixelatorViewMode::Parts) {
                ImGui::Spacing();
                ImGui::TextColored(ToolColors().secondary, "Parts");
                for (const auto& count : PixelatorPartCounts(image)) {
                    const ImVec2 row = ImGui::GetCursorScreenPos();
                    const float side = SwatchSize * scale;
                    ImGui::Dummy({rowWidth, side + 2.0f});
                    draw.AddRectFilled({row.x + 2.0f, row.y + 1.0f}, {row.x + 2.0f + side, row.y + 1.0f + side},
                                       U32(PixelatorIndexColor(count.index + 1)), 2.0f * scale);
                    const auto text = std::format("{:>3}  {} px", count.index, count.pixels);
                    draw.AddText({row.x + 2.0f + side + ToolMetrics::Gap * scale, row.y + 1.0f},
                                 ImGui::GetColorU32(count.index == 0 ? ToolColors().secondary : ToolColors().text),
                                 count.index == 0 ? (text + "  (none)").c_str() : text.c_str());
                }
            }
        }
        ImGui::EndChild();
        ImGui::SetCursorPosY(std::max(ImGui::GetCursorPosY(), ImGui::GetWindowHeight() - insetHeight));
        Inset(image, width, InsetHeight(image, size.y));
    }
    EndToolPanel();
}

void PixelatorPanel::FrameStrip() {
    const float scale = ToolUiScale();
    const auto& frames = _asset->frames;
    const bool several = frames.size() > 1;
    ImGui::BeginDisabled(!several);
    if (ToolIconButton("previous", ICON_FA_BACKWARD_STEP, "Previous frame (,)")) _playback.Step(frames, -1);
    ImGui::SameLine();
    if (ToolIconButton("play", _playback.Playing() ? ICON_FA_PAUSE : ICON_FA_PLAY,
                       _playback.Playing() ? "Pause" : "Play at the frames' own durations"))
        _playback.SetPlaying(!_playback.Playing());
    ImGui::SameLine();
    if (ToolIconButton("next", ICON_FA_FORWARD_STEP, "Next frame (.)")) _playback.Step(frames, 1);
    ImGui::EndDisabled();
    ImGui::SameLine();
    ImGui::AlignTextToFramePadding();
    const std::size_t frame = _playback.Frame();
    ImGui::PushFont(ToolFont(ToolFontRole::Technical), 0);
    if (frames.empty())
        ImGui::TextUnformatted("No frames");
    else
        ImGui::Text("Frame %zu / %zu  %s", frame + 1, frames.size(), Milliseconds(frames[frame].seconds).c_str());
    ImGui::PopFont();
    ImGui::SameLine();
    const std::string about = frames.size() == 1 ? "still"
                              : _asset->looped   ? "loops"
                                                 : std::format("first {:.1f} s", _asset->window);
    ImGui::TextColored(ToolColors().secondary, "%s%s", about.c_str(), _asset->capped ? ", cut at the limit" : "");
    ToolTooltip("Frames come from sampling the graphic's clock 60 times a second for up to 4 s: equal sprites in a "
                "row make one frame. A cycle that repeats is kept once (loops).");
    if (!several) return;
    ImGui::SameLine();
    const float width = std::max(1.0f, ImGui::GetContentRegionAvail().x);
    const float height = FrameBarHeight * scale;
    ImGui::SetCursorPosY(ImGui::GetCursorPosY() + (ImGui::GetFrameHeight() - height) * 0.5f);
    const ImVec2 start = ImGui::GetCursorScreenPos();
    ImGui::InvisibleButton("##frames", {width, height});
    float total = 0.0f;
    for (const auto& step : frames)
        total += std::max(step.seconds, 1.0f / 240.0f);
    auto& draw = *ImGui::GetWindowDrawList();
    float x = start.x;
    std::optional<std::size_t> hovered;
    for (std::size_t index = 0; index < frames.size(); ++index) {
        const float w = width * std::max(frames[index].seconds, 1.0f / 240.0f) / total;
        const bool current = index == frame;
        const ImVec4 color = current ? ToolColors().accent : index % 2 == 0 ? ToolColors().control : ToolColors().hover;
        draw.AddRectFilled({x, start.y}, {x + std::max(1.0f, w - 1.0f), start.y + height}, ImGui::GetColorU32(color));
        if (ImGui::IsItemHovered() && ImGui::GetIO().MousePos.x >= x && ImGui::GetIO().MousePos.x < x + w)
            hovered = index;
        x += w;
    }
    if (hovered) {
        ToolTooltip(std::format("Frame {}: {}", *hovered + 1, Milliseconds(frames[*hovered].seconds)).c_str());
        if (ImGui::IsItemClicked()) {
            _playback.SetPlaying(false);
            _playback.Select(frames, *hovered);
        }
    }
}

void PixelatorPanel::StatusBar(const PixelatorImage& image, const std::vector<Color4>& palette) {
    ImGui::PushFont(ToolFont(ToolFontRole::Technical), 0);
    std::string text = std::format("{} x {} px   {}x   pivot {}, {}", image.width, image.height, _view.Zoom(),
                                   image.pivot.x, image.pivot.y);
    if (_hoverPixel) {
        const Ink ink = image.InkAt(_hoverPixel->x, _hoverPixel->y);
        text += std::format("   x {}  y {}", _hoverPixel->x, _hoverPixel->y);
        if (ink == ClearInk)
            text += "   ink 0 (clear)";
        else
            text += std::format("   ink {}  {}   part {}", ink,
                                PixelatorHex(ink < palette.size() ? palette[ink] : Color4{0, 0, 0, 0}),
                                image.PartAt(_hoverPixel->x, _hoverPixel->y));
    }
    ImGui::AlignTextToFramePadding();
    ImGui::TextColored(ToolColors().secondary, "%s", text.c_str());
    ImGui::PopFont();
}

void PixelatorPanel::Draw(const EditorSnapshot& snapshot, const EditorUiActions& actions,
                          const EditorPixelatorKeys& keys) {
    _focused = ImGui::IsWindowFocused(ImGuiFocusedFlags_RootAndChildWindows) && !ImGui::GetIO().AppFocusLost;
    Adopt(snapshot.pixels);
    _projectArtPixel = std::clamp(snapshot.artPixel, 0, MaxGameScale);
    if (!_gameScaleChosen && _projectArtPixel > 0) _gameScale = _projectArtPixel;
    if (!_asset) {
        if (snapshot.pixelsPending)
            ToolEmptyState("Reading pixels", "The scene is sampling the graphic's frames.");
        else
            ToolEmptyState("No pixel graphic open",
                           "Open a graphic from Assets whose GraphicAsset has pixels to see its pixels here.");
        return;
    }
    if (!_asset->error.empty()) {
        ToolEmptyState(_asset->id.c_str(), _asset->error.c_str());
        return;
    }
    _playback.Advance(_asset->frames, ImGui::GetIO().DeltaTime);
    const auto& image = ImageOf(*_asset, _playback.Frame());
    const auto& palette = PaletteOf(*_asset, _playback.Frame());
    if (_focused && !ImGui::GetIO().WantTextInput && ImGui::IsKeyPressed(ImGuiKey_Escape, false)) _isolated.reset();
    ApplyKeys(keys, image);
    const float scale = ToolUiScale();
    const float padding = ToolMetrics::Gap;
    ImGui::PushStyleVar(ImGuiStyleVar_ItemSpacing, {ToolMetrics::Gap * scale, ToolMetrics::Gap * scale});
    if (BeginToolPanel("pixelator-toolbar", {0, 0}, ToolSurface::Toolbar,
                       ImGuiChildFlags_AutoResizeY | ImGuiChildFlags_AlwaysAutoResize, ImGuiWindowFlags_NoScrollbar,
                       padding))
        Toolbar(snapshot, actions, image);
    EndToolPanel();
    ImGui::SetCursorPosY(ImGui::GetCursorPosY() - ToolMetrics::Gap * scale);
    const float bottom = ImGui::GetFrameHeightWithSpacing() * 2 + padding * scale;
    const ImVec2 room = ImGui::GetContentRegionAvail();
    const float side = std::min(SideWidth * scale, room.x * 0.4f);
    const ImVec2 canvas{std::max(1.0f, room.x - side), std::max(1.0f, room.y - bottom)};
    if (image.Empty()) {
        ImGui::Dummy(canvas);
    } else {
        // The side strip sets the focused ink the canvas texture dims, so it is laid out first and drawn beside.
        const ImVec2 at = ImGui::GetCursorPos();
        ImGui::SetCursorPos({at.x + canvas.x, at.y});
        Side(image, palette, {side, canvas.y});
        Upload(actions, image, palette);
        ImGui::SetCursorPos(at);
        Canvas(image, palette, canvas);
    }
    if (BeginToolPanel("pixelator-bottom", {0, 0}, ToolSurface::Toolbar, 0, ImGuiWindowFlags_NoScrollbar, padding)) {
        FrameStrip();
        if (!image.Empty())
            StatusBar(image, palette);
        else
            ImGui::TextColored(ToolColors().secondary, "This frame has no pixels");
    }
    EndToolPanel();
    ImGui::PopStyleVar();
}

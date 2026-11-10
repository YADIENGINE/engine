#include "Editor/Ui/Panels/Pixelator/PixelatorPanel.h"
#include <extras/IconsFontAwesome6.h>
#include <imgui.h>
#include <algorithm>
#include <array>
#include <cmath>
#include <exception>
#include <format>
#include <string>
#include <type_traits>
#include <vector>
#include "Editor/Internals/Protocol/PixelatorProtocol.h"
#include "ToolUi/ToolTheme.h"
#include "ToolUi/ToolWidgets.h"

namespace {
constexpr int MaxGameScale = 8;           // Largest game-size preview factor offered
constexpr std::size_t SpriteSlot = 0;     // The view: colours per mode, focus dimmed (or the Diff view's)
constexpr std::size_t ColorSlot = 1;      // The game-size preview: plain colours
constexpr std::size_t CheckerSlot = 2;    // A 2x2 checker, repeated
constexpr std::size_t FirstPaneSlot = 3;  // Comparison panes after the first
constexpr std::size_t FirstGhostSlot = 6; // Onion-skin ghosts
constexpr int DiffLook = 100;             // SlotKey::look of the Diff view's colours
constexpr int BeforeLook = 101;           // ...and of the earlier build's frame
constexpr float SideWidth = 210.0f;
constexpr float CompactWidth = 760.0f; // Toolbar room below which it folds its rarer controls
constexpr float SwatchSize = 14.0f;
constexpr float InsetMaxHeight = 220.0f;
constexpr float InsetMaxShare = 0.4f; // Of the side strip's height; the ink list keeps the rest
constexpr float PivotArm = 7.0f;
constexpr float FrameBarHeight = 10.0f;
constexpr float PaneGap = 6.0f;
constexpr std::size_t MaxMarks = 4096; // Lint marks drawn on the canvas per frame
constexpr std::array TileChoices{0, 8, 16, 32};
constexpr std::array BackgroundNames{"Checker", "Dark", "Light"};
constexpr std::array ViewNames{"Color", "Inks", "Parts"};
constexpr std::array LayoutNames{"Frame", "Compare", "Tile 3x3", "Changes"};
constexpr std::array LayoutTips{"One frame, with onion skin and lint marks",
                                "2-4 items of the set side by side, zoom, pan, pointer and ink shared",
                                "The frame repeated 3 x 3, so a tile's seams show",
                                "What changed since the previous Build & Refresh: changed pixels bright, the rest dim"};
constexpr std::array ExportNames{"Frame", "Strip", "Grid", "GIF"};

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

void PixelatorPanel::Adopt(const std::shared_ptr<const PixelatorAsset>& asset, const EditorSnapshot& snapshot) {
    const bool changed = asset != _asset;
    if (changed) {
        const auto base = [](const std::string& id) { return id.substr(0, id.find('#')); };
        const bool other = !_asset || !asset || base(_asset->id) != base(asset->id);
        _asset = asset;
        _playback.Reset(_asset ? std::span<const PixelatorFrame>(_asset->frames) : std::span<const PixelatorFrame>{});
        _slotKeys = {};
        _findings = _asset ? LintPixelator(*_asset) : std::vector<PixelatorFinding>{};
        _finding.reset();
        if (other) {
            _isolated.reset();
            _paneItems = {};
            _exportResult.clear();
            _view = {}; // Fitted again for another graphic; the same graphic's other item keeps the zoom and place
        }
    }
    if (changed || snapshot.pixelsPrevious != _previous) {
        _previous = snapshot.pixelsPrevious;
        _diff.reset();
        if (_previous && _asset && _previous->id == _asset->id) _diff = DiffPixelator(*_previous, *_asset);
        _slotKeys[SpriteSlot] = {};
    }
}

const PixelatorFrameChange* PixelatorPanel::FrameChange() const {
    return _diff ? _diff->Frame(_playback.Frame()) : nullptr;
}

std::uint64_t PixelatorPanel::UploadSlot(const EditorUiActions& actions, std::size_t slot, const SlotKey& key,
                                         const PixelatorImage& image, const std::vector<Color4>& colors) {
    if (!actions.pixelatorTexture || slot >= _slotKeys.size() || image.Empty()) return 0;
    if (_slotKeys[slot] == key && _slotTextures[slot]) return _slotTextures[slot];
    _slotTextures[slot] = actions.pixelatorTexture(slot, image.width, image.height, colors);
    _slotKeys[slot] = key;
    return _slotTextures[slot];
}

void PixelatorPanel::Upload(const EditorUiActions& actions, const PixelatorImage& image,
                            const std::vector<Color4>& palette) {
    if (!actions.pixelatorTexture || image.Empty()) return;
    const std::size_t frame = _playback.Frame();
    const auto focus = Focus();
    if (_layout == Layout::Changes && _diff) {
        const bool before = _showBefore && frame < _previous->frames.size();
        const SlotKey key{_asset.get(), frame, before ? BeforeLook : DiffLook, std::nullopt};
        if (!(_slotKeys[SpriteSlot] == key) || !_slotTextures[SpriteSlot]) {
            if (before) {
                const auto& old = ImageOf(*_previous, frame);
                (void)UploadSlot(actions, SpriteSlot, key, old,
                                 PixelatorColors(old, PaletteOf(*_previous, frame), PixelatorViewMode::Color));
            } else {
                (void)UploadSlot(actions, SpriteSlot, key, image,
                                 PixelatorDiffColors(image, palette, FrameChange(), _diff->palette));
            }
        }
    } else {
        const SlotKey key{_asset.get(), frame, static_cast<int>(_mode), focus};
        if (!(_slotKeys[SpriteSlot] == key) || !_slotTextures[SpriteSlot])
            (void)UploadSlot(actions, SpriteSlot, key, image, PixelatorColors(image, palette, _mode, focus));
    }
    const SlotKey plain{_asset.get(), frame, -1, std::nullopt};
    if (!(_slotKeys[ColorSlot] == plain) || !_slotTextures[ColorSlot])
        (void)UploadSlot(actions, ColorSlot, plain, image, PixelatorColors(image, palette, PixelatorViewMode::Color));
    if (!_slotTextures[CheckerSlot]) {
        const std::array checker{PixelatorCheckerLight, PixelatorCheckerDark, PixelatorCheckerDark,
                                 PixelatorCheckerLight};
        _slotTextures[CheckerSlot] = actions.pixelatorTexture(CheckerSlot, 2, 2, checker);
    }
    _ghosts.clear();
    if (_onionOn && _layout == Layout::Single) {
        std::size_t slot = FirstGhostSlot;
        for (const auto& ghost : PixelatorOnionGhosts(_asset->frames.size(), frame, _asset->loops, _onion)) {
            const auto& ghostImage = ImageOf(*_asset, ghost.frame);
            const SlotKey key{_asset.get(), ghost.frame,
                              1000 + ghost.distance * 10 + (ghost.tint ? 1 : 0) +
                                  static_cast<int>(std::lround(ghost.opacity * 1000.0f)) * 100,
                              std::nullopt};
            const auto texture =
                (_slotKeys[slot] == key && _slotTextures[slot])
                    ? _slotTextures[slot]
                    : UploadSlot(actions, slot, key, ghostImage,
                                 PixelatorGhostColors(ghostImage, PaletteOf(*_asset, ghost.frame), ghost));
            if (texture) _ghosts.push_back({ghost, texture});
            if (++slot >= _slotKeys.size()) break;
        }
    }
}

void PixelatorPanel::ApplyKeys(const EditorPixelatorKeys& keys, const PixelatorImage& image) {
    const Vec2 size = ViewSize(image);
    const Float2 area{_canvasSize.x, _canvasSize.y};
    if (keys.fit) _view.Fit(area, size);
    if (keys.actual) _view.Actual(area, size);
    if (keys.zoom != 0) _view.ZoomAt(area * 0.5f, keys.zoom);
    if (keys.grid) _grid = !_grid;
    if (keys.frame != 0 && _asset) _playback.Step(_asset->frames, keys.frame);
}

Vec2 PixelatorPanel::ViewSize(const PixelatorImage& image) const {
    if (_layout == Layout::Tiled) return {image.width * PixelatorTileRepeat, image.height * PixelatorTileRepeat};
    if (_layout == Layout::Compare) {
        Vec2 largest{image.width, image.height};
        for (const auto& pane : _panes)
            if (pane.asset && !pane.asset->frames.empty()) {
                const auto& shown = ImageOf(*pane.asset, pane.frame);
                largest = {std::max(largest.x, shown.width), std::max(largest.y, shown.height)};
            }
        return largest;
    }
    return {image.width, image.height};
}

void PixelatorPanel::OnionPopup() {
    const float scale = ToolUiScale();
    ImGui::TextColored(ToolColors().secondary, "Onion skin");
    ImGui::SetNextItemWidth(160.0f * scale);
    (void)ToolSlider("Before##onion-before", _onion.before, 0, PixelatorOnionMaxFrames);
    ImGui::SetNextItemWidth(160.0f * scale);
    (void)ToolSlider("After##onion-after", _onion.after, 0, PixelatorOnionMaxFrames);
    ImGui::SetNextItemWidth(160.0f * scale);
    float percent = _onion.opacity * 100.0f;
    if (ToolSlider("Opacity##onion-opacity", percent, 5.0f, 90.0f, "%.0f %%")) _onion.opacity = percent / 100.0f;
    (void)ToolToggle("Tint earlier red, later blue", _onion.tinted);
}

void PixelatorPanel::ExportPopup(const EditorSnapshot& snapshot, const EditorUiActions& actions) {
    const float scale = ToolUiScale();
    ImGui::TextColored(ToolColors().secondary, "Export to Exports/%s/", PixelatorExportFolderName(*_asset).c_str());
    for (std::size_t index = 0; index < ExportNames.size(); ++index) {
        if (index > 0) ImGui::SameLine(0, 0);
        if (ToolSegment(ExportNames[index], static_cast<std::size_t>(_export.kind) == index))
            _export.kind = static_cast<PixelatorExportKind>(index);
    }
    ImGui::SetNextItemWidth(200.0f * scale);
    (void)ToolSlider("Scale##export-scale", _export.scale, 1, PixelatorExportMaxScale, "%dx");
    _export.frame = _playback.Frame();
    const auto file = PixelatorExportFileName(*_asset, _export);
    ImGui::TextColored(ToolColors().secondary, "%s, transparent background", file.c_str());
    ImGui::BeginDisabled(!actions.exportPixels || snapshot.projectDirectory.empty());
    if (ToolButton("Export", {}, true)) {
        try {
            const auto written = actions.exportPixels(_export);
            const auto text = written.generic_u8string();
            _exportResult = "Wrote " + std::string(reinterpret_cast<const char*>(text.data()), text.size());
            _exportFailed = false;
        } catch (const std::exception& error) {
            _exportResult = error.what();
            _exportFailed = true;
        }
    }
    ImGui::SameLine();
    const auto folder = PixelatorExportFolder(snapshot.projectDirectory, *_asset);
    ImGui::BeginDisabled(!actions.openPath || !std::filesystem::is_directory(folder));
    if (ToolButton("Open folder") && actions.openPath) actions.openPath(folder);
    ImGui::EndDisabled();
    ImGui::EndDisabled();
    if (!_exportResult.empty()) {
        ImGui::PushTextWrapPos(ImGui::GetCursorPosX() + 360.0f * scale);
        ImGui::TextColored(_exportFailed ? ToolToneColor(ToolTone::Error) : ToolColors().secondary, "%s",
                           _exportResult.c_str());
        ImGui::PopTextWrapPos();
    }
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
    // Narrow: the view and layout are lists and the rarer controls sit behind one button, so the canvas keeps its room.
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
    const auto combo = [&](const char* id, const auto& names, auto& value, const char* tip, float width) {
        ImGui::SetNextItemWidth(width * scale);
        if (ToolBeginCombo(id, names[static_cast<std::size_t>(value)])) {
            for (std::size_t index = 0; index < names.size(); ++index)
                if (ImGui::Selectable(names[index], static_cast<std::size_t>(value) == index))
                    value = static_cast<std::remove_reference_t<decltype(value)>>(index);
            ImGui::EndCombo();
        }
        ToolTooltip(tip);
    };
    if (compact) {
        combo("##pixelator-view", ViewNames, _mode,
              "Color: the palette's colours; Inks / Parts: a distinct colour per ink or part", 80.0f);
        ContinueToolToolbar(100.0f * scale);
        combo("##pixelator-layout", LayoutNames, _layout,
              "Frame, Compare items, Tile 3x3 or Changes since the last build", 100.0f);
    } else {
        for (std::size_t index = 0; index < ViewNames.size(); ++index) {
            if (index > 0) ImGui::SameLine(0, 0);
            if (ToolSegment(ViewNames[index], static_cast<std::size_t>(_mode) == index))
                _mode = static_cast<PixelatorViewMode>(index);
            ToolTooltip(index == 0   ? "The palette's colours"
                        : index == 1 ? "A distinct colour per ink"
                                     : "A distinct colour per part (PixelCanvas::Part)");
        }
        ContinueToolToolbar("Compare Tile 3x3 Changes Frame", true);
        for (std::size_t index = 0; index < LayoutNames.size(); ++index) {
            if (index > 0) ImGui::SameLine(0, 0);
            const bool attention = index == static_cast<std::size_t>(Layout::Changes) && _diff && !_diff->Same() &&
                                   _layout != Layout::Changes;
            if (ToolSegment(LayoutNames[index], static_cast<std::size_t>(_layout) == index,
                            attention ? ToolToneColor(ToolTone::Information) : ImVec4{0, 0, 0, 0}))
                _layout = static_cast<Layout>(index);
            ToolTooltip(LayoutTips[index]);
        }
    }
    if (_layout == Layout::Compare) {
        ContinueToolToolbar(80.0f * scale);
        ImGui::SetNextItemWidth(80.0f * scale);
        const auto label = std::format("{} panes", _paneCount);
        if (ToolBeginCombo("##pixelator-panes", label.c_str())) {
            for (int count = 2; count <= PixelatorMaxPanes; ++count)
                if (ImGui::Selectable(std::format("{} panes", count).c_str(), count == _paneCount)) _paneCount = count;
            ImGui::EndCombo();
        }
        ToolTooltip("How many items show side by side; pick each pane's item at its top");
    }
    if (_layout == Layout::Changes && _diff) {
        ContinueToolToolbar("Before", true);
        if (ToolSegment("Before", _showBefore)) _showBefore = !_showBefore;
        ToolTooltip("Show the previous build's frame instead of the changes");
    }
    ContinueToolToolbar(ImGui::GetFrameHeight() * 4.0f + ImGui::GetStyle().ItemSpacing.x * 3.0f, true);
    if (ToolIconButton("grid", ICON_FA_TABLE_CELLS, "Pixel grid (G); shown from 4x zoom", _grid)) _grid = !_grid;
    ImGui::SameLine();
    if (ToolIconButton("pivot", ICON_FA_CROSSHAIRS, "Pivot: the point the sprite is drawn at", _pivot))
        _pivot = !_pivot;
    ImGui::SameLine();
    ImGui::BeginDisabled(_asset->frames.size() < 2 || _layout != Layout::Single);
    if (ToolIconButton("onion", ICON_FA_LAYER_GROUP,
                       "Onion skin: earlier and later frames faintly under this one (right-click for settings)",
                       _onionOn))
        _onionOn = !_onionOn;
    if (ImGui::IsItemClicked(ImGuiMouseButton_Right)) ImGui::OpenPopup("pixelator-onion");
    ImGui::EndDisabled();
    if (ImGui::BeginPopup("pixelator-onion")) {
        OnionPopup();
        ImGui::EndPopup();
    }
    ImGui::SameLine();
    if (ToolIconButton("more", ICON_FA_ELLIPSIS,
                       compact ? "Tile grid, background, onion skin settings and export" : "Onion skin settings"))
        ImGui::OpenPopup("pixelator-more");
    bool openExport = false;
    if (ImGui::BeginPopup("pixelator-more")) {
        if (compact) {
            tilesCombo();
            backgroundCombo();
            if (ToolButton(ICON_FA_FILE_EXPORT " Export...")) {
                openExport = true;
                ImGui::CloseCurrentPopup();
            }
            ImGui::Separator();
        }
        OnionPopup();
        ImGui::EndPopup();
    }
    if (!compact) {
        ContinueToolToolbar(90.0f * scale, true);
        tilesCombo();
        ContinueToolToolbar(90.0f * scale);
        backgroundCombo();
    }
    const Vec2 size = ViewSize(image);
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
    // Lint and export: what the graphic is checked and saved as. Narrow: the count alone, export behind "...".
    const auto warnings = PixelatorWarnings(_findings);
    const auto lintLabel = compact ? std::format("{} {}###pixelator-lint", ICON_FA_LIST_CHECK, warnings)
                                   : std::format("{} Lint {}###pixelator-lint", ICON_FA_LIST_CHECK, warnings);
    ContinueToolToolbar(lintLabel.c_str(), !compact);
    if (ToolSegment(lintLabel.c_str(), _lintShown,
                    warnings > 0 && !_lintShown ? ToolToneColor(ToolTone::Warning) : ImVec4{0, 0, 0, 0}))
        _lintShown = !_lintShown;
    ToolTooltip(std::format("{} warning{}, {} note{}: pixel-art lint (python Engine asset lint). Click to list them "
                            "and mark them on the canvas.",
                            warnings, warnings == 1 ? "" : "s", _findings.size() - warnings,
                            _findings.size() - warnings == 1 ? "" : "s")
                    .c_str());
    if (!compact) {
        ImGui::SameLine();
        openExport = ToolIconButton("export", ICON_FA_FILE_EXPORT, "Export a frame, a strip, a grid or a GIF");
    }
    if (openExport) {
        _exportResult.clear();
        ImGui::OpenPopup("pixelator-export");
    }
    if (ImGui::BeginPopup("pixelator-export")) {
        ExportPopup(snapshot, actions);
        ImGui::EndPopup();
    }
}

void PixelatorPanel::Canvas(const PixelatorImage& image, ImVec2 size) {
    const float scale = ToolUiScale();
    const ImVec2 start = ImGui::GetCursorScreenPos();
    _canvasSize = size;
    const Vec2 sprite = ViewSize(image);
    const bool tiled = _layout == Layout::Tiled;
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
    const auto spot = hovered ? _view.PixelAt(mouse, sprite) : std::nullopt;
    _hoverPixel = spot ? std::optional<Vec2>(tiled ? PixelatorTiledPixel(*spot, {image.width, image.height}) : *spot)
                       : std::nullopt;

    auto& draw = *ImGui::GetWindowDrawList();
    const ImVec2 end = Add(start, size);
    draw.PushClipRect(start, end, true);
    draw.AddRectFilled(start, end, ImGui::GetColorU32(ToolColors().content));
    const float zoom = static_cast<float>(_view.Zoom());
    const ImVec2 origin = Add(start, {_view.Origin().x, _view.Origin().y});
    const ImVec2 extent{static_cast<float>(sprite.x) * zoom, static_cast<float>(sprite.y) * zoom};
    const ImVec2 spriteEnd = Add(origin, extent);
    if (_background == PixelatorBackground::Checker && _slotTextures[CheckerSlot]) {
        const float repeat = static_cast<float>(PixelatorCheckerSize(_view.Zoom()) * 2);
        draw.AddImage(ImTextureID(_slotTextures[CheckerSlot]), origin, spriteEnd, {0, 0},
                      {extent.x / repeat, extent.y / repeat});
    } else {
        draw.AddRectFilled(origin, spriteEnd, U32(PixelatorBackgroundColor(_background)));
    }
    // Onion skin under the frame, each ghost aligned on its pivot.
    for (const auto& [ghost, texture] : _ghosts) {
        const auto& ghostImage = ImageOf(*_asset, ghost.frame);
        const Vec2 offset = PixelatorPivotOffset(image, ghostImage);
        const ImVec2 at{origin.x + static_cast<float>(offset.x) * zoom, origin.y + static_cast<float>(offset.y) * zoom};
        draw.AddImage(
            ImTextureID(texture), at,
            Add(at, {static_cast<float>(ghostImage.width) * zoom, static_cast<float>(ghostImage.height) * zoom}));
    }
    if (const auto texture = _slotTextures[SpriteSlot]) {
        const bool before =
            _layout == Layout::Changes && _showBefore && _previous && _playback.Frame() < _previous->frames.size();
        const auto& shown = before ? ImageOf(*_previous, _playback.Frame()) : image;
        const Vec2 offset = before ? PixelatorPivotOffset(image, shown) : Vec2{};
        const ImVec2 tile{static_cast<float>(image.width) * zoom, static_cast<float>(image.height) * zoom};
        const int copies = tiled ? PixelatorTileRepeat : 1;
        for (int copyY = 0; copyY < copies; ++copyY)
            for (int copyX = 0; copyX < copies; ++copyX) {
                const ImVec2 at{origin.x + tile.x * static_cast<float>(copyX) + static_cast<float>(offset.x) * zoom,
                                origin.y + tile.y * static_cast<float>(copyY) + static_cast<float>(offset.y) * zoom};
                draw.AddImage(
                    ImTextureID(texture), at,
                    Add(at, {static_cast<float>(shown.width) * zoom, static_cast<float>(shown.height) * zoom}));
            }
    }
    // Only the lines inside the visible part are drawn: a 640-pixel sprite has hundreds.
    const auto visible = [&](int count, float from, float clipLow, float clipHigh) {
        const int first = std::max(1, static_cast<int>(std::floor((clipLow - from) / zoom)));
        const int last = std::min(count - 1, static_cast<int>(std::ceil((clipHigh - from) / zoom)));
        return std::pair{first, last};
    };
    const auto [firstColumn, lastColumn] = visible(sprite.x, origin.x, start.x, end.x);
    const auto [firstRow, lastRow] = visible(sprite.y, origin.y, start.y, end.y);
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
        for (const int x : PixelatorTileLines(sprite.x, _tiles)) {
            const float at = origin.x + static_cast<float>(x) * zoom;
            if (at >= start.x - 2 && at <= end.x + 2)
                draw.AddRectFilled({at - width + 1, top}, {at + 1.0f, bottom}, color);
        }
        for (const int y : PixelatorTileLines(sprite.y, _tiles)) {
            const float at = origin.y + static_cast<float>(y) * zoom;
            if (at >= start.y - 2 && at <= end.y + 2)
                draw.AddRectFilled({left, at - width + 1}, {right, at + 1.0f}, color);
        }
    }
    // Lint marks: every finding on this frame's sprite, the selected one brighter.
    if (_lintShown && _layout == Layout::Single && _asset && _playback.Frame() < _asset->frames.size()) {
        const auto shownImage = _asset->frames[_playback.Frame()].image;
        std::size_t drawn = 0;
        for (std::size_t index = 0; index < _findings.size() && drawn < MaxMarks; ++index) {
            const auto& finding = _findings[index];
            if (finding.image != shownImage) continue;
            const bool selected = _finding && *_finding == index;
            const ImU32 color = selected ? U32(PixelatorMarkColor) : IM_COL32(255, 224, 0, 150);
            for (const Vec2 pixel : finding.pixels) {
                const ImVec2 at{origin.x + static_cast<float>(pixel.x) * zoom,
                                origin.y + static_cast<float>(pixel.y) * zoom};
                draw.AddRect({at.x - 1.0f, at.y - 1.0f}, {at.x + zoom + 1.0f, at.y + zoom + 1.0f}, color, 0.0f, 0,
                             selected ? 2.0f : 1.0f);
                if (++drawn >= MaxMarks) break;
            }
        }
    }
    if (_pivot && !tiled) {
        const ImVec2 mark{origin.x + static_cast<float>(image.pivot.x) * zoom,
                          origin.y + static_cast<float>(image.pivot.y) * zoom};
        const float arm = PivotArm * scale;
        const ImU32 color = U32(PixelatorPivotColor);
        draw.AddRectFilled({mark.x - arm, mark.y - 1.0f}, {mark.x + arm, mark.y + 1.0f}, color);
        draw.AddRectFilled({mark.x - 1.0f, mark.y - arm}, {mark.x + 1.0f, mark.y + arm}, color);
    }
    if (spot) {
        const ImVec2 at{origin.x + static_cast<float>(spot->x) * zoom, origin.y + static_cast<float>(spot->y) * zoom};
        draw.AddRect({at.x - 1.0f, at.y - 1.0f}, {at.x + zoom + 1.0f, at.y + zoom + 1.0f}, IM_COL32(0, 0, 0, 200));
        draw.AddRect(at, {at.x + zoom, at.y + zoom}, IM_COL32(255, 255, 255, 230));
    }
    if (_layout == Layout::Changes && !_diff) {
        const char* note = "No earlier build of this graphic in this session. Change its code and Build & Refresh: "
                           "this view then shows what changed.";
        draw.AddRectFilled(Add(start, {8.0f, 8.0f}), Add(start, {size.x - 8.0f, 8.0f + ImGui::GetFrameHeight() * 1.6f}),
                           IM_COL32(0, 0, 0, 170), 4.0f);
        draw.AddText(Add(start, {16.0f, 12.0f}), ImGui::GetColorU32(ToolColors().text), note);
    }
    draw.PopClipRect();
}

void PixelatorPanel::CompareCanvas(const EditorSnapshot& snapshot, const EditorUiActions& actions, ImVec2 size) {
    const ImVec2 start = ImGui::GetCursorScreenPos();
    _canvasSize = size;
    const float scale = ToolUiScale();
    const auto panes = PixelatorPanes({size.x, size.y}, _paneCount, PaneGap * scale);
    // Pane 0 is the open item; the others default to the items after it.
    _panes.assign(panes.size(), {});
    _panes[0] = {_asset, _asset->items.empty() ? std::string() : _asset->items[_asset->item].name, _playback.Frame(),
                 0};
    for (std::size_t index = 1; index < panes.size(); ++index) {
        auto& id = _paneItems[index];
        if (id.empty() && !_asset->items.empty()) id = _asset->items[(_asset->item + index) % _asset->items.size()].id;
        const auto item = std::ranges::find(_asset->items, id, &PixelatorItem::id);
        _panes[index].name = item != _asset->items.end() ? item->name : id;
        if (const auto found = snapshot.pixelItems.find(id); found != snapshot.pixelItems.end())
            _panes[index].asset = found->second;
        if (id == _asset->id) _panes[index].asset = _asset;
        if (!_panes[index].asset && actions.requestPixels && !id.empty()) actions.requestPixels(id);
        if (_panes[index].asset && !_panes[index].asset->frames.empty())
            _panes[index].frame = _playback.Frame() % _panes[index].asset->frames.size();
    }
    // Textures: pane 0 uses the sprite slot.
    _panes[0].texture = _slotTextures[SpriteSlot];
    const auto focus = Focus();
    for (std::size_t index = 1; index < _panes.size(); ++index) {
        auto& pane = _panes[index];
        if (!pane.asset || !pane.asset->error.empty() || pane.asset->frames.empty()) continue;
        const auto& image = ImageOf(*pane.asset, pane.frame);
        pane.texture = UploadSlot(actions, FirstPaneSlot + index - 1,
                                  {pane.asset.get(), pane.frame, static_cast<int>(_mode), focus}, image,
                                  PixelatorColors(image, PaletteOf(*pane.asset, pane.frame), _mode, focus));
    }
    const Vec2 sprite = ViewSize(ImageOf(*_asset, _playback.Frame()));
    _view.Keep({panes[0].size.x, panes[0].size.y}, sprite);
    ImGui::InvisibleButton("##pixelator-canvas", size,
                           ImGuiButtonFlags_MouseButtonLeft | ImGuiButtonFlags_MouseButtonMiddle);
    const bool hovered = ImGui::IsItemHovered();
    const bool active = ImGui::IsItemActive();
    const auto& io = ImGui::GetIO();
    _hoverPixel.reset();
    std::optional<Vec2> spot;
    for (std::size_t index = 0; index < panes.size(); ++index) {
        const Float2 mouse{io.MousePos.x - start.x - panes[index].at.x, io.MousePos.y - start.y - panes[index].at.y};
        if (!hovered || mouse.x < 0 || mouse.y < 0 || mouse.x >= panes[index].size.x || mouse.y >= panes[index].size.y)
            continue;
        _hoverPane = index;
        if (io.MouseWheel != 0.0f) _view.ZoomAt(mouse, io.MouseWheel > 0.0f ? 1 : -1);
        spot = _view.PixelAt(mouse, sprite);
    }
    if (active && (ImGui::IsMouseDown(ImGuiMouseButton_Middle) ||
                   (ImGui::IsKeyDown(ImGuiKey_Space) && ImGui::IsMouseDown(ImGuiMouseButton_Left)))) {
        _view.Pan({io.MouseDelta.x, io.MouseDelta.y});
        ImGui::SetMouseCursor(ImGuiMouseCursor_ResizeAll);
    }
    auto& draw = *ImGui::GetWindowDrawList();
    const float zoom = static_cast<float>(_view.Zoom());
    for (std::size_t index = 0; index < panes.size(); ++index) {
        const ImVec2 paneStart = Add(start, {panes[index].at.x, panes[index].at.y});
        const ImVec2 paneEnd = Add(paneStart, {panes[index].size.x, panes[index].size.y});
        const auto& pane = _panes[index];
        draw.PushClipRect(paneStart, paneEnd, true);
        draw.AddRectFilled(paneStart, paneEnd, ImGui::GetColorU32(ToolColors().content));
        const ImVec2 origin = Add(paneStart, {_view.Origin().x, _view.Origin().y});
        if (pane.asset && pane.asset->error.empty() && !pane.asset->frames.empty()) {
            const auto& image = ImageOf(*pane.asset, pane.frame);
            const ImVec2 spriteEnd =
                Add(origin, {static_cast<float>(image.width) * zoom, static_cast<float>(image.height) * zoom});
            if (_background == PixelatorBackground::Checker && _slotTextures[CheckerSlot]) {
                const float repeat = static_cast<float>(PixelatorCheckerSize(_view.Zoom()) * 2);
                draw.AddImage(ImTextureID(_slotTextures[CheckerSlot]), origin, spriteEnd, {0, 0},
                              {(spriteEnd.x - origin.x) / repeat, (spriteEnd.y - origin.y) / repeat});
            } else {
                draw.AddRectFilled(origin, spriteEnd, U32(PixelatorBackgroundColor(_background)));
            }
            if (pane.texture) draw.AddImage(ImTextureID(pane.texture), origin, spriteEnd);
            if (spot && spot->x < image.width && spot->y < image.height) {
                if (index == _hoverPane) _hoverPixel = spot;
                const ImVec2 at{origin.x + static_cast<float>(spot->x) * zoom,
                                origin.y + static_cast<float>(spot->y) * zoom};
                draw.AddRect({at.x - 1.0f, at.y - 1.0f}, {at.x + zoom + 1.0f, at.y + zoom + 1.0f},
                             IM_COL32(0, 0, 0, 200));
                draw.AddRect(at, {at.x + zoom, at.y + zoom}, IM_COL32(255, 255, 255, 230));
            }
            if (_pivot) {
                const ImVec2 mark{origin.x + static_cast<float>(image.pivot.x) * zoom,
                                  origin.y + static_cast<float>(image.pivot.y) * zoom};
                const float arm = PivotArm * scale;
                draw.AddRectFilled({mark.x - arm, mark.y - 1.0f}, {mark.x + arm, mark.y + 1.0f},
                                   U32(PixelatorPivotColor));
                draw.AddRectFilled({mark.x - 1.0f, mark.y - arm}, {mark.x + 1.0f, mark.y + arm},
                                   U32(PixelatorPivotColor));
            }
        } else {
            const char* text = pane.asset ? pane.asset->error.c_str() : "Reading pixels...";
            draw.AddText(Add(paneStart, {12.0f, ImGui::GetFrameHeight() + 12.0f}),
                         ImGui::GetColorU32(ToolColors().secondary), text);
        }
        draw.PopClipRect();
    }
    // Each pane names its item; the panes after the first choose theirs.
    const ImVec2 after = ImGui::GetCursorScreenPos();
    for (std::size_t index = 0; index < panes.size(); ++index) {
        ImGui::SetCursorScreenPos(Add(start, {panes[index].at.x + 4.0f, panes[index].at.y + 4.0f}));
        ImGui::PushID(static_cast<int>(index));
        const auto& name = _panes[index].name.empty() ? std::string("(single drawing)") : _panes[index].name;
        if (index == 0 || _asset->items.empty()) {
            draw.AddRectFilled(ImGui::GetCursorScreenPos(),
                               Add(ImGui::GetCursorScreenPos(),
                                   {ImGui::CalcTextSize(name.c_str()).x + 12.0f, ImGui::GetFrameHeight()}),
                               IM_COL32(0, 0, 0, 160), 3.0f);
            ImGui::SetCursorScreenPos(Add(ImGui::GetCursorScreenPos(), {6.0f, ImGui::GetStyle().FramePadding.y}));
            ImGui::TextUnformatted(name.c_str());
        } else {
            ImGui::SetNextItemWidth(std::max(ImGui::CalcTextSize(name.c_str()).x + 40.0f * scale, 90.0f * scale));
            if (ToolBeginCombo("##pane-item", name.c_str())) {
                for (const auto& item : _asset->items)
                    if (ImGui::Selectable(item.name.c_str(), item.id == _paneItems[index])) _paneItems[index] = item.id;
                ImGui::EndCombo();
            }
        }
        ImGui::PopID();
    }
    ImGui::SetCursorScreenPos(after);
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
    if (_background == PixelatorBackground::Checker && _slotTextures[CheckerSlot]) {
        const float repeat = static_cast<float>(PixelatorCheckerCell * 2);
        draw.AddImage(ImTextureID(_slotTextures[CheckerSlot]), start, end, {0, 0}, {width / repeat, height / repeat});
    } else {
        draw.AddRectFilled(start, end, U32(PixelatorBackgroundColor(_background)));
    }
    const ImVec2 at{std::round(start.x + (width - sprite.x) * 0.5f), std::round(start.y + (height - sprite.y) * 0.5f)};
    if (_slotTextures[ColorSlot]) draw.AddImage(ImTextureID(_slotTextures[ColorSlot]), at, Add(at, sprite));
    draw.PopClipRect();
}

void PixelatorPanel::InkList(const PixelatorImage& image, const std::vector<Color4>& palette, float height) {
    const float scale = ToolUiScale();
    if (ImGui::BeginChild("pixelator-inks", {0, height})) {
        auto& draw = *ImGui::GetWindowDrawList();
        const float rowWidth = ImGui::GetContentRegionAvail().x; // Inside the list's scroll bar
        const std::optional<Ink> underPointer = _hoverPixel && _layout != Layout::Compare
                                                    ? std::optional<Ink>(image.InkAt(_hoverPixel->x, _hoverPixel->y))
                                                    : std::nullopt;
        for (const auto& count : PixelatorInkCounts(image)) {
            const auto ink = static_cast<Ink>(count.index);
            const Color4 color = _mode == PixelatorViewMode::Inks ? PixelatorIndexColor(ink)
                                 : ink < palette.size()           ? palette[ink]
                                                                  : Color4{0, 0, 0, 0};
            ImGui::PushID(count.index);
            const auto label = std::format("##ink{}", count.index);
            const bool selected = (_isolated && *_isolated == ink) || (underPointer && *underPointer == ink);
            const ImVec2 row = ImGui::GetCursorScreenPos();
            if (ImGui::Selectable(label.c_str(), selected, 0, {0, SwatchSize * scale + 2.0f}))
                _isolated = _isolated && *_isolated == ink ? std::nullopt : std::optional<Ink>(ink);
            if (ImGui::IsItemHovered()) {
                _hoverInk = ink;
                const Color4 actual = ink < palette.size() ? palette[ink] : Color4{0, 0, 0, 0};
                ToolTooltip(std::format("Ink {}: {}, {} pixels\nHover shows its pixels; click isolates it.",
                                        PixelatorInkLabel(*_asset, ink), PixelatorHex(actual), count.pixels)
                                .c_str());
            }
            const ImVec2 swatch{row.x + 2.0f, row.y + 1.0f};
            const float side = SwatchSize * scale;
            draw.AddRectFilled(swatch, Add(swatch, {side, side}), U32(color), 2.0f * scale);
            draw.AddRect(swatch, Add(swatch, {side, side}), ImGui::GetColorU32(ToolColors().separator), 2.0f * scale);
            const auto& name = _asset->InkName(ink);
            const auto text = name.empty() ? std::format("{:>3}  {}", count.index, PixelatorHex(color))
                                           : std::format("{:>3}  {}", count.index, name);
            draw.AddText({swatch.x + side + ToolMetrics::Gap * scale, row.y + 1.0f},
                         ImGui::GetColorU32(ToolColors().text), text.c_str());
            const auto pixels = std::to_string(count.pixels);
            draw.AddText(
                {row.x + rowWidth - ImGui::CalcTextSize(pixels.c_str()).x - ToolMetrics::Space * scale, row.y + 1.0f},
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
                const auto& name = _asset->PartName(count.index);
                const auto text = count.index == 0 ? std::format("{:>3}  {} px  (none)", count.index, count.pixels)
                                  : name.empty()   ? std::format("{:>3}  {} px", count.index, count.pixels)
                                                   : std::format("{:>3}  {}  {} px", count.index, name, count.pixels);
                draw.AddText({row.x + 2.0f + side + ToolMetrics::Gap * scale, row.y + 1.0f},
                             ImGui::GetColorU32(count.index == 0 ? ToolColors().secondary : ToolColors().text),
                             text.c_str());
            }
        }
    }
    ImGui::EndChild();
}

void PixelatorPanel::LintList(float height) {
    if (ImGui::BeginChild("pixelator-lint", {0, height})) {
        if (_findings.empty()) ImGui::TextColored(ToolColors().secondary, "No findings.");
        for (std::size_t index = 0; index < _findings.size(); ++index) {
            const auto& finding = _findings[index];
            ImGui::PushID(static_cast<int>(index));
            const bool warning = finding.severity == PixelatorLintSeverity::Warning;
            const bool selected = _finding && *_finding == index;
            ImGui::PushStyleColor(ImGuiCol_Text, warning ? ToolToneColor(ToolTone::Warning) : ToolColors().secondary);
            const auto label =
                std::format("{} {}", warning ? ICON_FA_TRIANGLE_EXCLAMATION : ICON_FA_CIRCLE_INFO, finding.rule);
            if (ImGui::Selectable(label.c_str(), selected)) {
                _finding = selected ? std::nullopt : std::optional<std::size_t>(index);
                // Show it: its first frame, the single view.
                if (_finding && !finding.frames.empty() && _asset) {
                    _playback.SetPlaying(false);
                    _playback.Select(_asset->frames, finding.frames.front());
                    _layout = Layout::Single;
                }
            }
            ImGui::PopStyleColor();
            if (ImGui::IsItemHovered()) ToolTooltip(finding.text.c_str());
            ImGui::PushTextWrapPos(0.0f);
            ImGui::TextColored(ToolColors().secondary, "%s", finding.text.c_str());
            ImGui::PopTextWrapPos();
            ImGui::PopID();
        }
    }
    ImGui::EndChild();
}

void PixelatorPanel::Side(const PixelatorImage& image, const std::vector<Color4>& palette, ImVec2 size) {
    _hoverInk.reset();
    if (BeginToolPanel("pixelator-side", size, ToolSurface::Utility, 0, ImGuiWindowFlags_NoScrollbar,
                       ToolMetrics::Gap)) {
        const float width = ImGui::GetContentRegionAvail().x;
        const float insetHeight = InsetHeight(image, size.y) + ImGui::GetFrameHeightWithSpacing() * 2;
        const auto counts = PixelatorInkCounts(image);
        if (ToolSegment(std::format("Inks {}###side-inks", counts.size()).c_str(), !_lintShown)) _lintShown = false;
        ImGui::SameLine(0, 0);
        const auto warnings = PixelatorWarnings(_findings);
        if (ToolSegment(std::format("Lint {}###side-lint", warnings).c_str(), _lintShown)) _lintShown = true;
        ToolTooltip("Pixel-art lint findings; selected ones are marked on the canvas");
        if (_isolated && !_lintShown) {
            ImGui::SameLine();
            if (ToolButton("Show all")) _isolated.reset();
            ToolTooltip("Stop isolating an ink (Esc)");
        }
        const float listHeight =
            std::max(ImGui::GetFrameHeight() * 3, size.y - insetHeight - ImGui::GetFrameHeight() * 2);
        if (_lintShown)
            LintList(listHeight);
        else
            InkList(image, palette, listHeight);
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
    const std::string about = frames.size() == 1 ? "still" : _asset->loops ? "loops" : "plays once";
    ImGui::TextColored(ToolColors().secondary, "%s%s", about.c_str(), _asset->capped ? ", cut at the size limit" : "");
    ToolTooltip("The frames the graphic's registration declares (GraphicAsset::pixels), with their durations.");
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
        const bool changed = _diff && _diff->Frame(index);
        const ImVec4 color = current          ? ToolColors().accent
                             : changed        ? ToolToneColor(ToolTone::Information)
                             : index % 2 == 0 ? ToolColors().control
                                              : ToolColors().hover;
        draw.AddRectFilled({x, start.y}, {x + std::max(1.0f, w - 1.0f), start.y + height}, ImGui::GetColorU32(color));
        if (ImGui::IsItemHovered() && ImGui::GetIO().MousePos.x >= x && ImGui::GetIO().MousePos.x < x + w)
            hovered = index;
        x += w;
    }
    if (hovered) {
        ToolTooltip(std::format("Frame {}: {}{}", *hovered + 1, Milliseconds(frames[*hovered].seconds),
                                _diff && _diff->Frame(*hovered) ? " (changed since the last build)" : "")
                        .c_str());
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
    // In comparison the pointer may be over another item's pane.
    const PixelatorAsset* asset = _asset.get();
    const PixelatorImage* shown = &image;
    const std::vector<Color4>* colors = &palette;
    if (_layout == Layout::Compare && _hoverPane < _panes.size() && _panes[_hoverPane].asset &&
        !_panes[_hoverPane].asset->frames.empty()) {
        asset = _panes[_hoverPane].asset.get();
        shown = &ImageOf(*asset, _panes[_hoverPane].frame);
        colors = &PaletteOf(*asset, _panes[_hoverPane].frame);
    }
    if (_hoverPixel && _hoverPixel->x < shown->width && _hoverPixel->y < shown->height) {
        const Ink ink = shown->InkAt(_hoverPixel->x, _hoverPixel->y);
        text += std::format("   x {}  y {}", _hoverPixel->x, _hoverPixel->y);
        if (ink == ClearInk) {
            text += "   ink 0 (clear)";
        } else {
            const PixelPart part = shown->PartAt(_hoverPixel->x, _hoverPixel->y);
            const auto& partName = asset->PartName(part);
            text += std::format("   ink {}  {}   part {}", PixelatorInkLabel(*asset, ink),
                                PixelatorHex(ink < colors->size() ? (*colors)[ink] : Color4{0, 0, 0, 0}),
                                partName.empty() ? std::to_string(part) : std::format("{} {}", part, partName));
        }
    }
    if (_layout == Layout::Changes && _diff) {
        if (const auto* change = FrameChange())
            text += std::format("   changed: {} pixels here", change->pixels.size());
        text += std::format("   since the last build: {} pixels in {} frame{}, {} colour{}", _diff->ChangedPixels(),
                            _diff->frames.size(), _diff->frames.size() == 1 ? "" : "s", _diff->palette.size(),
                            _diff->palette.size() == 1 ? "" : "s");
    }
    ImGui::AlignTextToFramePadding();
    ImGui::TextColored(ToolColors().secondary, "%s", text.c_str());
    ImGui::PopFont();
}

void PixelatorPanel::Draw(const EditorSnapshot& snapshot, const EditorUiActions& actions,
                          const EditorPixelatorKeys& keys) {
    _focused = ImGui::IsWindowFocused(ImGuiFocusedFlags_RootAndChildWindows) && !ImGui::GetIO().AppFocusLost;
    Adopt(snapshot.pixels, snapshot);
    _projectArtPixel = std::clamp(snapshot.artPixel, 0, MaxGameScale);
    if (!_gameScaleChosen && _projectArtPixel > 0) _gameScale = _projectArtPixel;
    if (!_asset) {
        if (snapshot.pixelsPending)
            ToolEmptyState("Reading pixels", "The scene is reading the graphic's frames.");
        else
            ToolEmptyState("No pixel graphic open",
                           "Open a graphic from Assets whose GraphicAsset has pixels to see its pixels here.");
        return;
    }
    if (!_asset->error.empty()) {
        ToolEmptyState(_asset->id.c_str(), _asset->error.c_str());
        return;
    }
    if (_layout == Layout::Compare && _asset->items.size() < 2) _layout = Layout::Single;
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
        if (_layout == Layout::Compare)
            CompareCanvas(snapshot, actions, canvas);
        else
            Canvas(image, canvas);
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

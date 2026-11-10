#pragma once
#include <imgui.h>
#include <array>
#include <cstddef>
#include <cstdint>
#include <memory>
#include <optional>
#include <string>
#include <vector>
#include "Editor/Application/EditorPresentation.h"
#include "Editor/Internals/Pixelator/PixelatorDiff.h"
#include "Editor/Internals/Pixelator/PixelatorExport.h"
#include "Editor/Internals/Pixelator/PixelatorLayout.h"
#include "Editor/Internals/Pixelator/PixelatorLint.h"
#include "Editor/Internals/Pixelator/PixelatorPicture.h"
#include "Editor/Internals/Pixelator/PixelatorPlayback.h"
#include "Editor/Internals/Pixelator/PixelatorView.h"
#include "Editor/Ui/Shell/EditorShell.h"

// The Pixelator: a pixel graphic's pixels (GraphicAsset::pixels), drawn by the editor from the data the worker sent
// (D64, D65). A canvas with whole-number zoom and pan, Color / Inks / Parts views, pixel and tile grids, the pivot,
// onion skin, item comparison, a 3x3 tile preview, the changes since the last build, lint findings, the inks it uses,
// its frames, a game-size preview and export. It only looks; the graphic's code stays the source.
class PixelatorPanel {
public:
    // `keys`: the Pixelator's shortcuts pressed this frame (EditorKeyScope::Pixelator).
    void Draw(const EditorSnapshot& snapshot, const EditorUiActions& actions, const EditorPixelatorKeys& keys);
    // The keyboard was in the panel during the last Draw.
    [[nodiscard]] bool Focused() const { return _focused; }

    // What the canvas shows: one frame, 2-4 items side by side, the frame repeated 3x3, or the changes since the
    // previous build.
    enum class Layout { Single, Compare, Tiled, Changes };
    // For tests and the UI smoke: the state a developer would reach by clicking.
    void ShowLayout(Layout layout) { _layout = layout; }
    void SetOnion(bool on) { _onionOn = on; }
    void ShowLint(bool on) { _lintShown = on; }

private:
    // What a texture slot holds, so it is uploaded again only when that changes.
    struct SlotKey {
        const PixelatorAsset* asset = nullptr;
        std::size_t frame = 0;
        int look = 0; // View mode, or a ghost's distance, or the Diff view
        std::optional<Ink> focus;
        bool operator==(const SlotKey&) const = default;
    };
    struct Pane {
        std::shared_ptr<const PixelatorAsset> asset;
        std::string name;
        std::size_t frame = 0;
        std::uint64_t texture = 0;
    };
    void Adopt(const std::shared_ptr<const PixelatorAsset>& asset, const EditorSnapshot& snapshot);
    void Toolbar(const EditorSnapshot& snapshot, const EditorUiActions& actions, const PixelatorImage& image);
    void OnionPopup();
    void ExportPopup(const EditorSnapshot& snapshot, const EditorUiActions& actions);
    void Canvas(const PixelatorImage& image, ImVec2 size);
    void CompareCanvas(const EditorSnapshot& snapshot, const EditorUiActions& actions, ImVec2 size);
    void Side(const PixelatorImage& image, const std::vector<Color4>& palette, ImVec2 size);
    void InkList(const PixelatorImage& image, const std::vector<Color4>& palette, float height);
    void LintList(float height);
    [[nodiscard]] float InsetHeight(const PixelatorImage& image, float sideHeight) const;
    void Inset(const PixelatorImage& image, float width, float height);
    void FrameStrip();
    void StatusBar(const PixelatorImage& image, const std::vector<Color4>& palette);
    void Upload(const EditorUiActions& actions, const PixelatorImage& image, const std::vector<Color4>& palette);
    std::uint64_t UploadSlot(const EditorUiActions& actions, std::size_t slot, const SlotKey& key,
                             const PixelatorImage& image, const std::vector<Color4>& colors);
    void ApplyKeys(const EditorPixelatorKeys& keys, const PixelatorImage& image);
    [[nodiscard]] Vec2 ViewSize(const PixelatorImage& image) const;
    [[nodiscard]] std::optional<Ink> Focus() const { return _hoverInk ? _hoverInk : _isolated; }
    [[nodiscard]] const PixelatorFrameChange* FrameChange() const;

    std::shared_ptr<const PixelatorAsset> _asset;
    std::shared_ptr<const PixelatorAsset> _previous; // The Diff view's earlier build
    std::optional<PixelatorDiff> _diff;
    std::vector<PixelatorFinding> _findings;
    PixelatorView _view;
    PixelatorPlayback _playback;
    Layout _layout = Layout::Single;
    PixelatorViewMode _mode = PixelatorViewMode::Color;
    PixelatorBackground _background = PixelatorBackground::Checker;
    bool _grid = true;
    bool _pivot = true;
    int _tiles = 0;
    bool _onionOn = false;
    PixelatorOnionOptions _onion;
    bool _lintShown = false;             // The side strip lists findings and the canvas marks them
    std::optional<std::size_t> _finding; // Selected in the list
    bool _showBefore = false;            // Changes: the earlier build instead of the highlighted current one
    int _paneCount = 2;
    std::array<std::string, PixelatorMaxPanes> _paneItems; // Comparison: item ids after the first pane
    PixelatorExportOptions _export;
    std::string _exportResult;
    bool _exportFailed = false;
    // Screen pixels per sprite pixel in the game-size preview: the project's art pixel (D63) until one is chosen.
    int _gameScale = 2;
    bool _gameScaleChosen = false;
    int _projectArtPixel = 0;     // The open scene's project; 0 in a smooth project
    std::optional<Ink> _isolated; // Clicked in the palette
    std::optional<Ink> _hoverInk; // Under the pointer in the palette, this frame
    std::optional<Vec2> _hoverPixel;
    std::size_t _hoverPane = 0; // Comparison: the pane under the pointer
    ImVec2 _canvasSize{};
    bool _focused = false;
    std::array<SlotKey, 16> _slotKeys{};
    std::array<std::uint64_t, 16> _slotTextures{};
    std::vector<Pane> _panes;
    std::vector<std::pair<PixelatorGhost, std::uint64_t>> _ghosts; // Textures of this frame's onion skin
};

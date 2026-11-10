#pragma once
#include <imgui.h>
#include <cstddef>
#include <cstdint>
#include <memory>
#include <optional>
#include "Editor/Application/EditorPresentation.h"
#include "Editor/Internals/Pixelator/PixelatorPicture.h"
#include "Editor/Internals/Pixelator/PixelatorPlayback.h"
#include "Editor/Internals/Pixelator/PixelatorView.h"
#include "Editor/Ui/Shell/EditorShell.h"

// The Pixelator: a pixel graphic's pixels (GraphicAsset::pixels), drawn by the editor from the data the worker sent
// (D64). A canvas with whole-number zoom and pan, Color / Inks / Parts views, pixel and tile grids, the pivot, the inks
// it uses, its frames and a game-size preview. It only looks; the graphic's code stays the source.
class PixelatorPanel {
public:
    // `keys`: the Pixelator's shortcuts pressed this frame (EditorKeyScope::Pixelator).
    void Draw(const EditorSnapshot& snapshot, const EditorUiActions& actions, const EditorPixelatorKeys& keys);
    // The keyboard was in the panel during the last Draw.
    [[nodiscard]] bool Focused() const { return _focused; }

private:
    struct Shown {
        const PixelatorAsset* asset = nullptr;
        std::size_t frame = 0;
        PixelatorViewMode mode = PixelatorViewMode::Color;
        std::optional<Ink> focus;
        bool operator==(const Shown&) const = default;
    };
    void Adopt(const std::shared_ptr<const PixelatorAsset>& asset);
    void Toolbar(const EditorSnapshot& snapshot, const EditorUiActions& actions, const PixelatorImage& image);
    void Canvas(const PixelatorImage& image, const std::vector<Color4>& palette, ImVec2 size);
    void Side(const PixelatorImage& image, const std::vector<Color4>& palette, ImVec2 size);
    [[nodiscard]] float InsetHeight(const PixelatorImage& image, float sideHeight) const;
    void Inset(const PixelatorImage& image, float width, float height);
    void FrameStrip();
    void StatusBar(const PixelatorImage& image, const std::vector<Color4>& palette);
    void Upload(const EditorUiActions& actions, const PixelatorImage& image, const std::vector<Color4>& palette);
    void ApplyKeys(const EditorPixelatorKeys& keys, const PixelatorImage& image);

    std::shared_ptr<const PixelatorAsset> _asset;
    PixelatorView _view;
    PixelatorPlayback _playback;
    PixelatorViewMode _mode = PixelatorViewMode::Color;
    PixelatorBackground _background = PixelatorBackground::Checker;
    bool _grid = true;
    bool _pivot = true;
    int _tiles = 0;
    // Hub: once project settings name the art pixel (pixel phase 2), start at that scale instead of 2.
    int _gameScale = 2;
    std::optional<Ink> _isolated; // Clicked in the palette
    std::optional<Ink> _hoverInk; // Under the pointer in the palette, this frame
    std::optional<Vec2> _hoverPixel;
    ImVec2 _canvasSize{};
    bool _focused = false;
    Shown _spriteShown; // What the view texture holds
    Shown _colorShown;  // What the game-size texture holds
    std::uint64_t _spriteTexture = 0;
    std::uint64_t _colorTexture = 0;
    std::uint64_t _checkerTexture = 0;
};

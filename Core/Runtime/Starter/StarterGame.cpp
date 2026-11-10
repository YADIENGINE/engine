#include <array>
#include <cmath>
#include <memory>
#include "Graphics/Paint/Painter.h"
#include "Graphics/Pixel/PixelCanvas.h"
#include "Graphics/Pixel/PixelSprite.h"
#include "Graphics/Text/TextStyle.h"
#include "Input/InputNavigation.h"
#include "Runtime/EntryPoint.h"
#include "Runtime/Game.h"

namespace {
constexpr int CanvasWidth = 1280;
constexpr int CanvasHeight = 720;
constexpr int TitleSize = 48;
constexpr int HintSize = 24;
constexpr int LineGap = 64;
constexpr Color4 TitleColor{245, 245, 245, 255};
constexpr Color4 HintColor{130, 130, 130, 255};
constexpr float SpriteAbove = 120.0f; // Canvas units between the sprite's feet and the title

// An example pixel sprite (Docs/Graphics/PixelArt.md): shapes the kit shades from the light, a stamped face, an
// outline.
enum : Ink { Line = 1, Dark, Mid, Light, Shine, Eye };
PixelSprite StarterSprite(int frame) {
    constexpr int Width = 16;
    constexpr int Height = 16;
    PixelCanvas c(Width, Height);
    const auto squash = static_cast<float>(frame);
    constexpr InkRamp Body{Dark, Dark, Mid, Light, Shine};
    c.Ellipse(8.0f, 9.5f + squash * 0.5f, 6.5f + squash * 0.5f, 6.0f - squash * 0.5f, Body);
    c.Stamp(5, 8 + frame, {"6..6"});
    c.Outline(Line);
    return PixelSprite(c, {Width / 2, Height});
}
const PixelPalette StarterPalette{Rgb(0x1b1824), Rgb(0x2f4a6b), Rgb(0x4d86a8),
                                  Rgb(0x8ccbd9), Rgb(0xf2f0e6), Rgb(0x1b1824)};

GameSettings Settings() {
    GameSettings settings;
    settings.project.applicationId = ENGINE_NAME "Starter";
    settings.project.title = ENGINE_NAME " starter";
    settings.project.canvasWidth = CanvasWidth;
    settings.project.canvasHeight = CanvasHeight;
    settings.persistentOptions = false;
    settings.project.font = "Fonts/Starter.ttf";
    return settings;
}

// engine_add_game builds this in place of the project's CreateGame while Game/ has no sources.
class StarterGame final : public Game {
public:
    StarterGame()
        : Game(Settings())
        , _frames{StarterSprite(0), StarterSprite(1)} {}

private:
    void HandleInput(const DeviceInput& input) override {
        if (InputNavigation(input).Back()) RequestQuit();
    }
    void Draw(Painter& paint) const override {
        constexpr int CenterX = CanvasWidth / 2;
        constexpr int CenterY = CanvasHeight / 2;
        // Each sprite pixel is one art pixel: Game.settings.json "artPixel" canvas units.
        const auto frame = static_cast<std::size_t>(std::fmod(paint.Time() * 2.0, 2.0));
        paint.Sprite(_frames[frame], StarterPalette, {CenterX, CenterY - LineGap - SpriteAbove + TitleSize});
        paint.Text("Game/ has no sources yet", {CenterX, CenterY - LineGap}, CenteredText(TitleSize, TitleColor));
        paint.Text("Add Game/CreateGame.cpp: Engine/Docs/GettingStarted.md", {CenterX, CenterY},
                   CenteredText(HintSize, HintColor));
        paint.Text("Esc quits", {CenterX, CenterY + LineGap}, CenteredText(HintSize, HintColor));
    }

    std::array<PixelSprite, 2> _frames;
};
} // namespace

std::unique_ptr<Game> CreateGame() { return std::make_unique<StarterGame>(); }

#include "Scenes/PlayScene.h"
#include <string>
#include <variant>
#include "Graphics/Paint/Painter.h"
#include "Graphics/Text/TextStyle.h"
#include "Input/Input.h"
#include "Input/InputNavigation.h"
#include "Runtime/TimeScale.h"
#include "Simulation/ArenaRules.h"
#include "Ui/Widgets/UiButton.h"
#include "Ui/Widgets/UiLabel.h"
#include "Ui/Widgets/UiLayer.h"
#include "Ui/Widgets/UiPanel.h"
#include "Utilities/Overloaded.h"

namespace {
constexpr float HitstopSeconds = 0.08f;
constexpr int BlinkSteps = 6;
constexpr float HudSize = 28.0f;
constexpr float TitleSize = 56.0f;
constexpr Float2 HudPosition{56.0f, 52.0f};
constexpr Color4 Background = Rgb(0x10131a);
constexpr Color4 FloorColor = Rgb(0x1b2130);
constexpr Color4 WallColor = Rgb(0x52607a);
constexpr Color4 PlayerColor = Rgb(0x6fe3ff);
constexpr Color4 EnemyColor = Rgb(0xff5a5a);
constexpr Color4 CoinColor = Rgb(0xffd34d);
constexpr Color4 TextColor = Rgb(0xf2f2f2);

InputBindings MakeBindings() {
    InputBindings bindings(1);
    bindings.Bind(0, ArenaButton::Left, InputKey::A);
    bindings.Bind(0, ArenaButton::Right, InputKey::D);
    bindings.Bind(0, ArenaButton::Up, InputKey::W);
    bindings.Bind(0, ArenaButton::Down, InputKey::S);
    bindings.Bind(0, ArenaButton::Left, InputKey::Left);
    bindings.Bind(0, ArenaButton::Right, InputKey::Right);
    bindings.Bind(0, ArenaButton::Up, InputKey::Up);
    bindings.Bind(0, ArenaButton::Down, InputKey::Down);
    return bindings;
}
} // namespace

PlayScene::PlayScene(const SceneServices& services, std::uint64_t seed)
    : Scene(TimeDomain::Game)
    , _time(services.time)
    , _font(services.font)
    , _start{.seed = seed, .players = 1}
    , _simulation(_start)
    , _runner(_simulation, ArenaSettings(), _start)
    , _bindings(MakeBindings()) {}

void PlayScene::HandleInput(const DeviceInput& input) {
    if (_paused) {
        _ui.Input(input);
        return;
    }
    if (InputNavigation(input).Back()) {
        _paused = true;
        _runner.CancelInput();
        return;
    }
    if (_simulation.Over() && InputNavigation(input).Confirm()) _runner.Command("restart", 0);
    _runner.Feed(_bindings.Read(input));
}

void PlayScene::InputCancelled() { _runner.CancelInput(); }

void PlayScene::Update(float seconds) {
    if (_font) _ui.SetFont(_font);
    _ui.Begin(seconds);
    PauseMenu();
    _ui.End();
    if (_paused) return;
    (void)_runner.Advance(seconds);
    for (const auto& happened : _events.Read(_simulation.Events(), _runner))
        std::visit(Overloaded{[](const CoinTaken&) {}, [&](const PlayerHit&) { _time.Hitstop(HitstopSeconds); },
                              [](const GameOver&) {}},
                   happened.event);
}

void PlayScene::Draw(Painter& paint) const {
    paint.Rect(paint.Bounds(), Fill(Background));
    const FloatRect arena(ArenaLeft, ArenaTop, ArenaRight - ArenaLeft, ArenaBottom - ArenaTop);
    paint.Rect(arena, Fill(FloorColor));
    paint.Rect(arena, Outline(4.0f, WallColor));
    paint.Circle(_simulation.Coin(), CoinRadius, Fill(CoinColor));
    for (const Float2 enemy : _simulation.Enemies())
        paint.Circle(enemy, EnemyRadius, Fill(EnemyColor));
    const bool blinkOff = _simulation.Invulnerable() && (_runner.StepIndex() / BlinkSteps) % 2 == 1;
    if (!blinkOff) paint.Circle(_simulation.Player(), PlayerRadius, Fill(PlayerColor));

    const std::string hud =
        "Score " + std::to_string(_simulation.Score()) + "    Lives " + std::to_string(_simulation.Lives());
    paint.Text(hud, HudPosition, FittedText(HudSize, TextColor));
    if (_simulation.Over()) {
        const Float2 center{(ArenaLeft + ArenaRight) * 0.5f, (ArenaTop + ArenaBottom) * 0.5f};
        paint.Text("Game over", center, CenteredText(TitleSize, TextColor));
        paint.Text("Enter to play again", center + Float2{0.0f, TitleSize}, CenteredText(HudSize, TextColor));
    }
    _ui.Draw(paint);
}

void PlayScene::PauseMenu() {
    if (auto pause = UiLayer(_ui, "pause", _paused, {.alignX = UiAlign::Center, .alignY = UiAlign::Center})) {
        auto panel = UiPanel(_ui, "panel", {.alignX = UiAlign::Center});
        UiLabel(_ui, "Paused", {.size = TitleSize});
        if (UiButton(_ui, "resume", "Resume", {.width = UiFill()}) || _ui.Back()) _paused = false;
        if (UiButton(_ui, "quit", "Quit", {.width = UiFill()})) _quit = true;
    }
}

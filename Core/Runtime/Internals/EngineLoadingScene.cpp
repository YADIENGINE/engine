#include "Runtime/Internals/EngineLoadingScene.h"
#include <algorithm>
#include "Graphics/Paint/Painter.h"
#include "Input/Input.h"
#include "Input/InputNavigation.h"
#include "Runtime/Internals/LoadingScreens/LoadingScreenFrame.h"
#include "Runtime/Internals/LoadingScreens/LoadingScreens.h"

namespace {
constexpr float FadeInSeconds = 0.4f;
constexpr float HoldSeconds = 1.2f;
constexpr float FadeOutSeconds = 0.4f;
constexpr float SkipFadeSeconds = 0.2f;
} // namespace

EngineLoadingScene::EngineLoadingScene(Vec2 canvas, ProjectSettings::LoadingScreen look)
    : _canvas(canvas)
    , _look(look)
    , _pixels({static_cast<float>(LoadingScreenPixelSize.x), static_cast<float>(LoadingScreenPixelSize.y)},
              {.smooth = false}) {}

bool EngineLoadingScene::Finished() const { return Fading() && _fadeElapsed >= _fadeSeconds; }
float EngineLoadingScene::Visibility() const {
    if (Fading()) return _fadeFrom * (1.0f - _fadeElapsed / _fadeSeconds);
    return std::min(_seconds / FadeInSeconds, 1.0f);
}
void EngineLoadingScene::BeginFade(float seconds) {
    _fadeFrom = Visibility();
    _fadeSeconds = seconds;
}
void EngineLoadingScene::Update(float seconds) {
    _seconds += seconds;
    if (Fading())
        _fadeElapsed = std::min(_fadeElapsed + seconds, _fadeSeconds);
    else if (Loaded() && _seconds >= FadeInSeconds + HoldSeconds)
        BeginFade(FadeOutSeconds);
}
void EngineLoadingScene::HandleInput(const DeviceInput& input) {
    if (Fading() || !Loaded()) return;
    const InputNavigation navigation(input);
    if (navigation.AnyPressed() || navigation.PressPoint()) BeginFade(SkipFadeSeconds);
}

void EngineLoadingScene::Draw(Painter& paint) const {
    paint.Post() = PostSettings{}; // A game's finish (bloom, tape ...) is for its own scenes
    const LoadingScreenFrame frame{_canvas, _seconds, Visibility(), _pixels};
    using Look = ProjectSettings::LoadingScreen;
    switch (_look) {
    case Look::Default: DrawDefaultLoadingScreen(paint, frame); break;
    case Look::Horror : DrawHorrorLoadingScreen(paint, frame); break;
    case Look::Rpg    : DrawRpgLoadingScreen(paint, frame); break;
    case Look::SciFi  : DrawSciFiLoadingScreen(paint, frame); break;
    case Look::Arcade : DrawArcadeLoadingScreen(paint, frame); break;
    case Look::Cozy   : DrawCozyLoadingScreen(paint, frame); break;
    }
}

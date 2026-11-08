#include "Runtime/Internals/EngineLoadingScene.h"
#include <algorithm>
#include <cmath>
#include "Graphics/Paint/Painter.h"
#include "Input/Input.h"
#include "Input/InputNavigation.h"

namespace {
constexpr float FadeInSeconds = 0.4f;
constexpr float HoldSeconds = 1.2f;
constexpr float FadeOutSeconds = 0.4f;
constexpr float SkipFadeSeconds = 0.2f;

constexpr Color4 Background = Rgb(0x050907);
constexpr Color4 Mark = Rgb(0xbac9bf);
constexpr float MarkWidth = 96.0f;
constexpr float MarkHeight = 8.0f;
constexpr float TrackWidth = 320.0f;
constexpr float SlideSeconds = 1.1f;
} // namespace

EngineLoadingScene::EngineLoadingScene(Vec2 canvas)
    : _canvas(canvas) {}

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

// A stand-in picture: its only job until the real one is drawn is to show the timing and that the screen is alive.
void EngineLoadingScene::Draw(Painter& paint) const {
    paint.Post() = PostSettings{}; // A game's finish (bloom, tape ...) is for its own scenes
    const auto width = static_cast<float>(_canvas.x);
    const auto height = static_cast<float>(_canvas.y);
    paint.Rect({0.0f, 0.0f, width, height}, Fill(Background));
    const float slide = std::abs(std::fmod(_seconds / SlideSeconds, 2.0f) - 1.0f);
    const float left = (width - TrackWidth) * 0.5f + slide * (TrackWidth - MarkWidth);
    paint.Rect({left, (height - MarkHeight) * 0.5f, MarkWidth, MarkHeight}, Fill(Mark));
    paint.Rect({0.0f, 0.0f, width, height}, Fill(WithAlpha(Background, 1.0f - Visibility())));
}

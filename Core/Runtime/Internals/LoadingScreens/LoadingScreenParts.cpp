#include "Runtime/Internals/LoadingScreens/LoadingScreenParts.h"
#include <cmath>
#include "Graphics/Paint/Painter.h"
#include "Runtime/Internals/LoadingScreens/LoadingScreenFrame.h"

namespace {
constexpr float MarkWidth = 96.0f;
constexpr float MarkHeight = 8.0f;
constexpr float TrackWidth = 320.0f;
constexpr float SlideSeconds = 1.1f;
} // namespace

void DrawLoadingStandIn(Painter& paint, const LoadingScreenFrame& frame, Color4 background, Color4 mark) {
    const auto width = static_cast<float>(frame.canvas.x);
    const auto height = static_cast<float>(frame.canvas.y);
    paint.Rect({0.0f, 0.0f, width, height}, Fill(background));
    const float slide = std::abs(std::fmod(frame.seconds / SlideSeconds, 2.0f) - 1.0f);
    const float left = (width - TrackWidth) * 0.5f + slide * (TrackWidth - MarkWidth);
    paint.Rect({left, (height - MarkHeight) * 0.5f, MarkWidth, MarkHeight}, Fill(mark));
    paint.Rect({0.0f, 0.0f, width, height}, Fill(WithAlpha(Rgb(0x000000), 1.0f - frame.visibility)));
}

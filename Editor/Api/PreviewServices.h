#pragma once
#include <functional>
#include <string>
#include "Utilities/Vec2.h"
#include "Editor/Api/PreviewParameter.h"

class AudioSystem;
class TextRenderer;
class Profiler;

// References outlive the preview. Never retain these services after Stop/destruction.
// Presentation time is the engine draw clock (DrawTime), which the preview host pauses and scales.
struct PreviewServices {
    Vec2 canvas;
    const TextRenderer& text;
    AudioSystem& audio;
    std::function<void(const std::string&)> log;
    std::function<std::string(const std::string&, const PreviewParameters&)> applicationCommand;
    Profiler* profiler = nullptr;
};

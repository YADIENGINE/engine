#pragma once
#include <cstddef>
#include <initializer_list>
#include <span>
#include <vector>

// One step of a pixel animation: which sprite (an index the game chooses, usually into a PixelSpriteSet) shows for how
// many seconds.
struct PixelFrame {
    std::size_t sprite = 0;
    float seconds = 0.1f;
};

// The frames of one pixel animation as data: sprites and how long each shows. The game picks the sprite for its clock
// with SpriteAt; the editor's Pixelator reads the same frames (GraphicPixels::Animated), so both always agree.
//     const PixelAnimation Walk = PixelAnimation::Even(4, 8.0f);       // Sprites 0-3, 8 frames a second, looping
//     const PixelAnimation Blink{{{0, 2.0f}, {1, 0.1f}}};               // Eyes open for 2 s, shut for 0.1 s
//     paint.Sprite(sprites[Walk.SpriteAt(time)], palette, feet);
class PixelAnimation {
public:
    // At least one frame, each longer than 0 seconds; otherwise invalid_argument.
    PixelAnimation(std::initializer_list<PixelFrame> frames, bool loops = true);
    explicit PixelAnimation(std::vector<PixelFrame> frames, bool loops = true);
    // Sprites first, first + 1, ... first + count - 1, each 1 / framesPerSecond seconds.
    [[nodiscard]] static PixelAnimation Even(std::size_t count, float framesPerSecond, std::size_t first = 0,
                                             bool loops = true);

    // The frame showing at `time` seconds (an index into Frames()). A looping animation repeats; one that does not
    // loop holds its last frame. Times before 0 show the first frame.
    [[nodiscard]] std::size_t FrameAt(float time) const;
    // The sprite showing at `time`.
    [[nodiscard]] std::size_t SpriteAt(float time) const { return _frames[FrameAt(time)].sprite; }
    [[nodiscard]] std::span<const PixelFrame> Frames() const { return _frames; }
    // Seconds of one pass.
    [[nodiscard]] float Length() const { return _length; }
    [[nodiscard]] bool Loops() const { return _loops; }

private:
    std::vector<PixelFrame> _frames;
    float _length = 0.0f;
    bool _loops = true;
};

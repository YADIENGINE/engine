#pragma once
#include "Utilities/Color4.h"

// Bloom: emissive drawing (style emission > 0) glows past its edges. Intensity 0 turns it off.
struct BloomSettings {
    float intensity = 0.0f; // Light added on top, 1: as bright as the source
    float radius = 1.0f;    // Spread multiplier, 0.25 .. 4
};

// Film grain that never repeats, stronger in the dark than on bright things such as text.
struct GrainSettings {
    float amount = 0.0f; // 0..1; subtle up to 0.15, heavy at 0.4
    float size = 1.0f;   // One grain, in canvas units
};

// Colour multiplied over the image: a dark, cold colour gives a dark, cold look.
struct TintSettings {
    Color4 color{255, 255, 255, 255}; // Alpha is ignored
    float amount = 0.0f;              // 0: no tint, 1: the whole colour
};

// The image slowly bends and tilts. It fades out toward the edges, so a HUD along them stays still.
struct WobbleSettings {
    float amount = 0.0f; // Largest move, in canvas units; uneasy at 3, hard to read at 12
    float speed = 1.0f;  // Rhythm multiplier; it can change while the effect runs
};

// Worn video tape: a tracking band that rolls down the image, and lines that jump sideways.
struct TapeSettings {
    float band = 0.0f;  // Tracking band 0..1
    float shift = 0.0f; // Largest sideways jump of a line, in canvas units; text stays readable up to about 2
    float speed = 1.0f; // How fast the band rolls; it can change while the effect runs
};

// Full-screen finish applied to the canvas when the frame is presented. A game sets its defaults
// (GameSettings::post); a frame can change them through Painter::Post() (they return to the defaults next frame).
// It covers everything drawn on the canvas, the HUD too. Grain, wobble and tape go off when the player turns
// RenderQuality::screenMotion off; a game does not check for that.
struct PostSettings {
    BloomSettings bloom;
    float vignette = 0.0f;      // Corner darkening 0..1
    float scanlines = 0.0f;     // Horizontal line darkening 0..1
    float scanlineCount = 0.0f; // Lines over the canvas height (0: one per two canvas units)
    float aberration = 0.0f;    // Chromatic split at the edges, in canvas units
    float fade = 0.0f;          // Colour drained toward grey 0..1
    TintSettings tint;
    GrainSettings grain;
    WobbleSettings wobble;
    TapeSettings tape;
};

// Between two finishes, 't' clamped to 0..1: paint.Post() = Mix(calm, disturbed, danger). This is also what lets a
// Tween or a clip animate a whole PostSettings. An effect that is off on one side keeps the other side's colour, size
// and speed, so that only its strength changes. A scanline count of 0 changes to a set count halfway; lines don't blend.
[[nodiscard]] PostSettings Mix(const PostSettings& from, const PostSettings& to, float t);

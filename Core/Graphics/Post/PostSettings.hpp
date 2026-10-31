#pragma once

// Bloom: emissive drawing (style emission > 0) glows past its edges. Intensity 0 turns it off.
struct BloomSettings {
    float intensity = 0.0f; // Light added on top, 1: as bright as the source
    float radius = 1.0f;    // Spread multiplier, 0.25 .. 4
};

// Full-screen finish applied to the canvas when the frame is presented. A game sets its defaults
// (GameSettings::post); a frame can change them through Painter::Post() (they return to the defaults next frame).
struct PostSettings {
    BloomSettings bloom;
    float vignette = 0.0f;      // Corner darkening 0..1
    float scanlines = 0.0f;     // Horizontal line darkening 0..1
    float scanlineCount = 0.0f; // Lines over the canvas height (0: one per two canvas units)
    float aberration = 0.0f;    // Chromatic split at the edges, in canvas units
};

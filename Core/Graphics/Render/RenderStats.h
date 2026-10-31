#pragma once
#include <cstdint>

// What the last presented frame cost the renderer (Game::RenderStatistics, the editor's profiler).
struct RenderStats {
    std::uint32_t drawCalls = 0;
    std::uint32_t batches = 0; // Recorded draw batches (each is one draw call)
    std::uint32_t passes = 0;  // Render target switches, post-processing included
    std::uint32_t vertices = 0;
    std::uint32_t indices = 0;
    std::uint64_t uploadedBytes = 0; // Geometry and texture updates
    std::uint32_t pipelineChanges = 0;
    std::uint32_t scissorChanges = 0;
    std::uint32_t textureBinds = 0;
    std::uint32_t glyphsRasterized = 0;  // Glyphs that were not cached yet
    std::uint32_t layoutHits = 0;        // Text layouts reused this frame
    std::uint32_t layoutMisses = 0;      // Text layouts computed this frame
    std::uint32_t surfacesAllocated = 0; // Surface / group images (re)allocated
    std::uint32_t paintErrors = 0;       // Drawing calls skipped as invalid (Release builds; Debug throws)
    float gpuMilliseconds = 0.0f;        // GPU time of the frame (a few frames late; 0 when unknown)
};

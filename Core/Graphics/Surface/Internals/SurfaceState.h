#pragma once
#include <memory>
#include <vector>
#include "Graphics/Gpu/Internals/GpuDevice.h"
#include "Graphics/Surface/Surface.h"

// GPU objects released by their owners while a frame may still use them; the renderer destroys them after the
// frame. Shared by the renderer's resources and every surface: when the renderer goes away 'device' becomes null
// and later releases do nothing (the device already freed everything).
struct GpuRetirement {
    GpuDevice* device = nullptr;
    std::vector<GpuTarget> targets;
    std::vector<GpuTexture> textures;
};

struct Surface::State {
    Float2 size;
    SurfaceOptions options;
    std::shared_ptr<GpuRetirement> retirement; // Set at first allocation
    GpuTarget target;
    GpuTexture color;
    GpuTexture emission;
    int pixelWidth = 0;
    int pixelHeight = 0;
    Float2 allocatedScale; // Canvas pixel scale the target was allocated for
    bool drawn = false;    // Holds a complete image for the current allocation

    void Release() {
        if (target.Valid() && retirement && retirement->device) retirement->targets.push_back(target);
        target = {};
        color = {};
        emission = {};
        pixelWidth = 0;
        pixelHeight = 0;
        drawn = false;
    }
};

struct SurfaceAccess {
    using State = Surface::State;
    [[nodiscard]] static Surface::State& Of(Surface& surface) { return *surface._state; }
    [[nodiscard]] static const Surface::State& Of(const Surface& surface) { return *surface._state; }
};

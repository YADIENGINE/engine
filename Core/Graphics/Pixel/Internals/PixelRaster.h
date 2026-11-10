#pragma once
#include <algorithm>
#include <cmath>
#include <cstdlib>
#include <span>
#include <vector>
#include "Utilities/Geometry/Vec2.h"

// Rasterization rules shared by PixelMask and PixelCanvas: a shape covers a pixel when it covers its centre
// (x + 0.5, y + 0.5). Each calls 'plot(x, y)' for the covered pixels inside width x height.
struct PixelRaster {
    // Even-odd scanlines through pixel centres.
    template <class Plot> static void Polygon(std::span<const Vec2> points, int width, int height, Plot&& plot) {
        if (points.size() < 3) return;
        std::vector<float> crossings;
        for (int y = 0; y < height; ++y) {
            const float scan = static_cast<float>(y) + 0.5f;
            crossings.clear();
            for (std::size_t index = 0; index < points.size(); ++index) {
                const Vec2 a = points[index];
                const Vec2 b = points[(index + 1) % points.size()];
                const auto ay = static_cast<float>(a.y);
                const auto by = static_cast<float>(b.y);
                if ((ay <= scan) == (by <= scan)) continue;
                crossings.push_back(static_cast<float>(a.x) + (scan - ay) / (by - ay) * static_cast<float>(b.x - a.x));
            }
            std::sort(crossings.begin(), crossings.end());
            for (std::size_t pair = 0; pair + 1 < crossings.size(); pair += 2) {
                const int from = std::max(0, static_cast<int>(std::ceil(crossings[pair] - 0.5f)));
                const int to = std::min(width - 1, static_cast<int>(std::floor(crossings[pair + 1] - 0.5f)));
                for (int x = from; x <= to; ++x)
                    plot(x, y);
            }
        }
    }

    template <class Plot>
    static void Ellipse(float cx, float cy, float rx, float ry, int width, int height, Plot&& plot) {
        if (!(rx > 0.0f) || !(ry > 0.0f)) return;
        for (int y = 0; y < height; ++y) {
            for (int x = 0; x < width; ++x) {
                const float nx = (static_cast<float>(x) + 0.5f - cx) / rx;
                const float ny = (static_cast<float>(y) + 0.5f - cy) / ry;
                if (nx * nx + ny * ny <= 1.0f) plot(x, y);
            }
        }
    }

    template <class Plot>
    static void Limb(float x0, float y0, float x1, float y1, float r0, float r1, int width, int height, Plot&& plot) {
        const float dx = x1 - x0;
        const float dy = y1 - y0;
        const float length = dx * dx + dy * dy;
        for (int y = 0; y < height; ++y) {
            for (int x = 0; x < width; ++x) {
                const float px = static_cast<float>(x) + 0.5f - x0;
                const float py = static_cast<float>(y) + 0.5f - y0;
                const float t = length > 0.0f ? std::clamp((px * dx + py * dy) / length, 0.0f, 1.0f) : 0.0f;
                const float ex = px - dx * t;
                const float ey = py - dy * t;
                const float radius = r0 + (r1 - r0) * t;
                if (ex * ex + ey * ey <= radius * radius) plot(x, y);
            }
        }
    }

    // Bresenham, both ends included; points outside a grid are the caller's to skip.
    template <class Plot> static void Line(int x0, int y0, int x1, int y1, Plot&& plot) {
        const int dx = std::abs(x1 - x0);
        const int dy = -std::abs(y1 - y0);
        const int stepX = x0 < x1 ? 1 : -1;
        const int stepY = y0 < y1 ? 1 : -1;
        int error = dx + dy;
        while (true) {
            plot(x0, y0);
            if (x0 == x1 && y0 == y1) break;
            const int twice = 2 * error;
            if (twice >= dy) {
                error += dy;
                x0 += stepX;
            }
            if (twice <= dx) {
                error += dx;
                y0 += stepY;
            }
        }
    }
};

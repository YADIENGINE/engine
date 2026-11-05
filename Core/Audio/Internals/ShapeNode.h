#pragma once
#include <miniaudio.h>
#include <memory>
#include "Audio/Internals/AudioShaper.h"

struct ShapeNodeData;

// An AudioShaper as a stop in miniaudio's graph. Sounds and other nodes attach to its input; attaching several mixes
// them. A node that always runs (a bus) keeps its fades moving even with nothing playing through it. A voice's stops
// with its sound, which is what holds a paused voice's fade in place.
class ShapeNode {
public:
    enum class Running { WhileFed, Always };

    ShapeNode(ma_engine& engine, ma_node* output, Running running);
    ~ShapeNode();

    [[nodiscard]] ma_node* Node();
    [[nodiscard]] AudioShaper& Shaper();
    [[nodiscard]] const AudioShaper& Shaper() const;

private:
    std::unique_ptr<ShapeNodeData> _data;

public:
    ShapeNode(const ShapeNode&) = delete;
    ShapeNode& operator=(const ShapeNode&) = delete;
};

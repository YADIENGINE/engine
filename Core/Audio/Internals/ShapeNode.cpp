#include "Audio/Internals/ShapeNode.h"
#include <algorithm>
#include <stdexcept>
#include <string>

struct ShapeNodeData {
    explicit ShapeNodeData(float rate)
        : shaper(rate) {}
    ma_node_base base{}; // First: miniaudio hands this back as the node
    AudioShaper shaper;
};

namespace {
void ProcessShape(ma_node* node, const float** in, ma_uint32* inFrames, float** out, ma_uint32* outFrames) {
    const float* input = in != nullptr ? in[0] : nullptr;
    const ma_uint32 frames = input != nullptr ? std::min(*inFrames, *outFrames) : *outFrames;
    static_cast<ShapeNodeData*>(node)->shaper.Process(input, out[0], frames);
    if (input != nullptr) *inFrames = frames;
    *outFrames = frames;
}

// miniaudio keeps pointers to these, so they outlive every graph.
ma_node_vtable WhileFedTable{ProcessShape, nullptr, 1, 1, 0};
ma_node_vtable AlwaysTable{ProcessShape, nullptr, 1, 1, MA_NODE_FLAG_CONTINUOUS_PROCESSING | MA_NODE_FLAG_ALLOW_NULL_INPUT};
} // namespace

ShapeNode::ShapeNode(ma_engine& engine, ma_node* output, Running running)
    : _data(std::make_unique<ShapeNodeData>(static_cast<float>(ma_engine_get_sample_rate(&engine)))) {
    const ma_uint32 channels[]{2};
    ma_node_config config = ma_node_config_init();
    config.vtable = running == Running::Always ? &AlwaysTable : &WhileFedTable;
    config.pInputChannels = channels;
    config.pOutputChannels = channels;
    const ma_result made = ma_node_init(ma_engine_get_node_graph(&engine), &config, nullptr, &_data->base);
    if (made != MA_SUCCESS)
        throw std::runtime_error(std::string("Could not make an audio node: ") + ma_result_description(made));
    ma_node_attach_output_bus(&_data->base, 0, output, 0);
}

ShapeNode::~ShapeNode() { ma_node_uninit(&_data->base, nullptr); }

ma_node* ShapeNode::Node() { return &_data->base; }
AudioShaper& ShapeNode::Shaper() { return _data->shaper; }
const AudioShaper& ShapeNode::Shaper() const { return _data->shaper; }

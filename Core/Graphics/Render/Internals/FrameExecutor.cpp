#include "Graphics/Render/Internals/FrameExecutor.h"
#include <span>
#include <stdexcept>
#include <type_traits>

FrameExecutor::FrameExecutor(GpuDevice& device)
    : _device(device) {}

void FrameExecutor::ResetCache() {
    _blend.reset();
    _scissor.reset();
    _textures.clear();
    _texturesKnown = false;
}

void FrameExecutor::Begin(const PaintTarget& target, std::optional<PremultipliedColor> clear) {
    GpuPassDesc pass;
    pass.target = target.target;
    pass.viewport = target.viewport;
    pass.viewSize = target.size;
    pass.clear = clear;
    _device.BeginPass(pass);
    _passOpen = true;
    ResetCache();
}

void FrameExecutor::Execute(DrawList& list) {
    if (!list.HasPending()) return;
    const std::size_t vertexStart = list.ExecutedVertices();
    const std::size_t indexStart = list.ExecutedIndices();
    GpuGeometry geometry;
    if (list.Indices().size() > indexStart) {
        const std::span<const PaintVertex> vertices(list.Vertices().data() + vertexStart,
                                                    list.Vertices().size() - vertexStart);
        const std::span<const std::uint32_t> indices(list.Indices().data() + indexStart,
                                                     list.Indices().size() - indexStart);
        // Indices are frame-wide; the device wants them relative to this upload.
        _indices.clear();
        for (const std::uint32_t index : indices)
            _indices.push_back(index - static_cast<std::uint32_t>(vertexStart));
        geometry = _device.Upload(vertices, _indices);
    }
    const auto& commands = list.Commands();
    for (std::size_t i = list.ExecutedCommands(); i < commands.size(); ++i) {
        std::visit(
            [&](const auto& command) {
                using Command = std::decay_t<decltype(command)>;
                if constexpr (std::is_same_v<Command, BeginTargetCommand>) {
                    if (_passOpen) _device.EndPass();
                    _targets.push_back(command.target);
                    Begin(command.target, command.clear);
                } else if constexpr (std::is_same_v<Command, EndTargetCommand>) {
                    if (_targets.empty()) throw std::logic_error("FrameExecutor: no target to end");
                    if (_passOpen) _device.EndPass();
                    _passOpen = false;
                    _targets.pop_back();
                    if (!_targets.empty()) Begin(_targets.back(), std::nullopt);
                } else if constexpr (std::is_same_v<Command, PushMarkerCommand>) {
                    _device.PushMarker(command.name);
                } else if constexpr (std::is_same_v<Command, PopMarkerCommand>) {
                    _device.PopMarker();
                } else {
                    if (command.indexCount == 0) return;
                    if (!_passOpen) {
                        if (_targets.empty()) throw std::logic_error("FrameExecutor: drawing without a target");
                        Begin(_targets.back(), std::nullopt);
                    }
                    if (_blend != command.blend) {
                        _device.SetPipeline(GpuProgram::Paint, command.blend);
                        _blend = command.blend;
                    }
                    if (!_scissor || *_scissor != command.scissor) {
                        _device.SetScissor(command.scissor ? &*command.scissor : nullptr);
                        _scissor = command.scissor;
                    }
                    const std::span<const GpuTexture> textures(command.textures.data(), command.textureCount);
                    const bool sameTextures = _texturesKnown && std::equal(textures.begin(), textures.end(),
                                                                           _textures.begin(), _textures.end());
                    if (!sameTextures && !textures.empty()) {
                        _device.BindTextures(textures);
                        _textures.assign(textures.begin(), textures.end());
                        _texturesKnown = true;
                    }
                    _device.DrawIndexed(geometry, command.firstIndex - static_cast<std::uint32_t>(indexStart),
                                        command.indexCount);
                    ++_batches;
                }
            },
            commands[i]);
    }
    list.MarkExecuted();
    // A target flush or native tool overlay may follow: leave no pass open.
    if (_passOpen) {
        _device.EndPass();
        _passOpen = false;
    }
}

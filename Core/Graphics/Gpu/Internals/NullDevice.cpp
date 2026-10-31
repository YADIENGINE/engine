#include "Graphics/Gpu/Internals/NullDevice.h"
#include <stdexcept>

namespace {
std::size_t BytesPerPixel(GpuFormat format) {
    switch (format) {
    case GpuFormat::R8     : return 1;
    case GpuFormat::Rgba8  : return 4;
    case GpuFormat::Rgba16F: return 8;
    }
    return 4;
}

void Require(bool condition, const char* message) {
    if (!condition) throw std::logic_error(message);
}
} // namespace

GpuTexture NullDevice::AddTexture(const GpuTextureDesc& desc, bool owned) {
    Require(desc.width > 0 && desc.height > 0, "NullDevice: texture needs a positive size");
    std::uint32_t index = 0;
    for (std::uint32_t i = 1; i < _textures.size(); ++i) {
        if (!_textures[i].live) {
            index = i;
            break;
        }
    }
    if (index == 0) {
        index = static_cast<std::uint32_t>(_textures.size());
        _textures.emplace_back();
    }
    TextureSlot& slot = _textures[index];
    ++slot.generation;
    slot.live = true;
    slot.owned = owned;
    slot.desc = desc;
    slot.desc.label = nullptr;
    slot.pixels.assign(static_cast<std::size_t>(desc.width) * static_cast<std::size_t>(desc.height) *
                           BytesPerPixel(desc.format),
                       std::byte{0});
    return {index, slot.generation};
}

NullDevice::TextureSlot& NullDevice::Live(GpuTexture texture) {
    Require(texture.Valid() && texture.index < _textures.size(), "NullDevice: unknown texture handle");
    TextureSlot& slot = _textures[texture.index];
    Require(slot.live && slot.generation == texture.generation, "NullDevice: stale texture handle");
    return slot;
}

const NullDevice::TextureSlot& NullDevice::Live(GpuTexture texture) const {
    return const_cast<NullDevice*>(this)->Live(texture);
}

const NullDevice::TargetSlot& NullDevice::Live(GpuTarget target) const {
    Require(target.Valid() && target.index < _targets.size(), "NullDevice: unknown target handle");
    const TargetSlot& slot = _targets[target.index];
    Require(slot.live && slot.generation == target.generation, "NullDevice: stale target handle");
    return slot;
}

GpuTexture NullDevice::CreateTexture(const GpuTextureDesc& desc) { return AddTexture(desc, true); }

void NullDevice::UpdateTexture(GpuTexture texture, PixelRect region, std::span<const std::byte> pixels) {
    TextureSlot& slot = Live(texture);
    Require(region.x >= 0 && region.y >= 0 && region.width >= 0 && region.height >= 0 &&
                region.x + region.width <= slot.desc.width && region.y + region.height <= slot.desc.height,
            "NullDevice: update region outside the texture");
    const std::size_t pixel = BytesPerPixel(slot.desc.format);
    const auto rowBytes = static_cast<std::size_t>(region.width) * pixel;
    Require(pixels.size() == rowBytes * static_cast<std::size_t>(region.height), "NullDevice: pixel size mismatch");
    for (int row = 0; row < region.height; ++row) {
        const std::size_t to = (static_cast<std::size_t>(region.y + row) * static_cast<std::size_t>(slot.desc.width) +
                                static_cast<std::size_t>(region.x)) *
                               pixel;
        const auto from = pixels.subspan(static_cast<std::size_t>(row) * rowBytes, rowBytes);
        std::copy(from.begin(), from.end(), slot.pixels.begin() + static_cast<std::ptrdiff_t>(to));
    }
    _counters.uploadedBytes += pixels.size();
}

void NullDevice::DestroyTexture(GpuTexture texture) {
    TextureSlot& slot = Live(texture);
    Require(slot.owned, "NullDevice: a target's attachment is destroyed with the target");
    slot.live = false;
    slot.pixels.clear();
}

GpuTarget NullDevice::CreateTarget(const GpuTargetDesc& desc) {
    Require(desc.width > 0 && desc.height > 0, "NullDevice: target needs a positive size");
    std::uint32_t index = 0;
    for (std::uint32_t i = 1; i < _targets.size(); ++i) {
        if (!_targets[i].live) {
            index = i;
            break;
        }
    }
    if (index == 0) {
        index = static_cast<std::uint32_t>(_targets.size());
        _targets.emplace_back();
    }
    const GpuTextureDesc texture{desc.width, desc.height, desc.format, desc.filter, nullptr};
    TargetSlot& slot = _targets[index];
    ++slot.generation;
    slot.live = true;
    slot.desc = desc;
    slot.desc.label = nullptr;
    slot.color = AddTexture(texture, false);
    slot.emission = desc.emission ? AddTexture(texture, false) : GpuTexture{};
    return {index, slot.generation};
}

GpuTexture NullDevice::TargetTexture(GpuTarget target, int attachment) const {
    const TargetSlot& slot = Live(target);
    Require(attachment == 0 || (attachment == 1 && slot.emission.Valid()), "NullDevice: no such attachment");
    return attachment == 0 ? slot.color : slot.emission;
}

void NullDevice::DestroyTarget(GpuTarget target) {
    (void)Live(target);
    TargetSlot& slot = _targets[target.index];
    for (const GpuTexture texture : {slot.color, slot.emission}) {
        if (!texture.Valid()) continue;
        _textures[texture.index].live = false;
        _textures[texture.index].pixels.clear();
    }
    slot.live = false;
}

GpuNativeTarget NullDevice::Native(GpuTarget target) const {
    const TargetSlot& slot = Live(target);
    return {target.index, slot.color.index, slot.desc.width, slot.desc.height};
}

void NullDevice::BeginFrame() {
    Require(!_inFrame, "NullDevice: frame already begun");
    _inFrame = true;
    _counters = {};
    ++_frame;
}

void NullDevice::EndFrame() {
    Require(_inFrame && !_inPass && _markerDepth == 0, "NullDevice: frame ended with an open pass or marker");
    _inFrame = false;
}

GpuGeometry NullDevice::Upload(std::span<const PaintVertex> vertices, std::span<const std::uint32_t> indices) {
    Require(_inFrame, "NullDevice: upload outside a frame");
    const GpuGeometry geometry{static_cast<std::int64_t>(_vertices.size()),
                               static_cast<std::uint32_t>(_indices.size())};
    _vertices.insert(_vertices.end(), vertices.begin(), vertices.end());
    _indices.insert(_indices.end(), indices.begin(), indices.end());
    _counters.uploadedBytes += vertices.size_bytes() + indices.size_bytes();
    NullCommand command{.type = NullCommandType::Upload};
    command.geometry = geometry;
    command.indexCount = static_cast<std::uint32_t>(indices.size());
    Record(std::move(command));
    return geometry;
}

void NullDevice::BeginPass(const GpuPassDesc& pass) {
    Require(_inFrame && !_inPass, "NullDevice: pass outside a frame or inside another pass");
    if (pass.target.Valid()) (void)Live(pass.target);
    _inPass = true;
    ++_counters.passes;
    NullCommand command{.type = NullCommandType::BeginPass};
    command.pass = pass;
    Record(std::move(command));
}

void NullDevice::EndPass() {
    Require(_inPass, "NullDevice: no pass to end");
    _inPass = false;
    Record({.type = NullCommandType::EndPass});
}

void NullDevice::SetPipeline(GpuProgram program, GpuBlend blend) {
    Require(_inPass, "NullDevice: pipeline outside a pass");
    ++_counters.pipelineChanges;
    NullCommand command{.type = NullCommandType::SetPipeline};
    command.program = program;
    command.blend = blend;
    Record(std::move(command));
}

void NullDevice::SetScissor(const PixelRect* rect) {
    Require(_inPass, "NullDevice: scissor outside a pass");
    ++_counters.scissorChanges;
    NullCommand command{.type = NullCommandType::SetScissor};
    if (rect) command.scissor = *rect;
    Record(std::move(command));
}

void NullDevice::BindTextures(std::span<const GpuTexture> textures) {
    Require(textures.size() <= PaintTextureSlots, "NullDevice: too many textures");
    for (const GpuTexture texture : textures)
        (void)Live(texture);
    _counters.textureBinds += static_cast<std::uint32_t>(textures.size());
    NullCommand command{.type = NullCommandType::BindTextures};
    command.textures.assign(textures.begin(), textures.end());
    Record(std::move(command));
}

void NullDevice::DrawIndexed(const GpuGeometry& geometry, std::uint32_t firstIndex, std::uint32_t indexCount) {
    Require(_inPass, "NullDevice: draw outside a pass");
    Require(geometry.firstIndex + firstIndex + indexCount <= _indices.size(), "NullDevice: draw past the upload");
    ++_counters.drawCalls;
    NullCommand command{.type = NullCommandType::DrawIndexed};
    command.geometry = geometry;
    command.firstIndex = firstIndex;
    command.indexCount = indexCount;
    Record(std::move(command));
}

void NullDevice::DrawFullscreen(const PostUniforms& uniforms) {
    Require(_inPass, "NullDevice: draw outside a pass");
    ++_counters.drawCalls;
    NullCommand command{.type = NullCommandType::DrawFullscreen};
    command.post = uniforms;
    Record(std::move(command));
}

void NullDevice::ReleaseState() {
    Require(!_inPass, "NullDevice: state released inside a pass");
    Record({.type = NullCommandType::Release});
}

void NullDevice::PushMarker(std::string_view name) {
    ++_markerDepth;
    NullCommand command{.type = NullCommandType::PushMarker};
    command.marker = std::string(name);
    Record(std::move(command));
}

void NullDevice::PopMarker() {
    Require(_markerDepth > 0, "NullDevice: no marker to pop");
    --_markerDepth;
    Record({.type = NullCommandType::PopMarker});
}

std::vector<const NullCommand*> NullDevice::CommandsOf(NullCommandType type) const {
    std::vector<const NullCommand*> found;
    for (const NullCommand& command : _commands)
        if (command.type == type) found.push_back(&command);
    return found;
}

std::vector<PaintVertex> NullDevice::DrawnVertices(const NullCommand& draw) const {
    std::vector<PaintVertex> vertices;
    const std::uint32_t first = draw.geometry.firstIndex + draw.firstIndex;
    for (std::uint32_t i = first; i < first + draw.indexCount; ++i)
        vertices.push_back(_vertices[static_cast<std::size_t>(draw.geometry.baseVertex + _indices[i])]);
    return vertices;
}

const GpuTextureDesc& NullDevice::Describe(GpuTexture texture) const { return Live(texture).desc; }

const GpuTargetDesc& NullDevice::Describe(GpuTarget target) const { return Live(target).desc; }

const std::vector<std::byte>& NullDevice::Pixels(GpuTexture texture) const { return Live(texture).pixels; }

std::size_t NullDevice::LiveTextures() const {
    std::size_t count = 0;
    for (const TextureSlot& slot : _textures)
        count += slot.live && slot.owned ? 1 : 0;
    return count;
}

std::size_t NullDevice::LiveTargets() const {
    std::size_t count = 0;
    for (const TargetSlot& slot : _targets)
        count += slot.live ? 1 : 0;
    return count;
}

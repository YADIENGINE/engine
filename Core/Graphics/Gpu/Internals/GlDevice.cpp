#include "Graphics/Gpu/Internals/GlDevice.h"
#include "external/glad.h"
#include <algorithm>
#include <cstdint>
#include <cstdio>
#include <cstring>
#include <stdexcept>
#include <string>
#include <string_view>
#include "EngineShaders.h" // Generated from Shaders/*.glsl (Core/CMakeLists.txt)

namespace {
constexpr std::size_t InitialVertexBytes = std::size_t{4} << 20;
constexpr std::size_t InitialIndexBytes = std::size_t{1} << 20;
constexpr GLsizei Stride = sizeof(PaintVertex);

struct Format {
    GLint internal;
    GLenum layout;
    GLenum type;
    std::size_t bytes;
};
Format FormatOf(GpuFormat format) {
    switch (format) {
    case GpuFormat::R8     : return {GL_R8, GL_RED, GL_UNSIGNED_BYTE, 1};
    case GpuFormat::Rgba8  : return {GL_RGBA8, GL_RGBA, GL_UNSIGNED_BYTE, 4};
    case GpuFormat::Rgba16F: return {GL_RGBA16F, GL_RGBA, GL_HALF_FLOAT, 8};
    }
    return {GL_RGBA8, GL_RGBA, GL_UNSIGNED_BYTE, 4};
}

template <class Texture> void DeleteTexture(const Texture& texture) { glDeleteTextures(1, &texture.id); }

void Label(GLenum kind, GLuint id, const char* label) {
    if (label && glad_glObjectLabel) glObjectLabel(kind, id, -1, label);
}

GLuint Compile(GLenum stage, std::string_view source) {
    const GLuint shader = glCreateShader(stage);
    const GLchar* text = source.data();
    const auto length = static_cast<GLint>(source.size());
    glShaderSource(shader, 1, &text, &length);
    glCompileShader(shader);
    GLint ok = 0;
    glGetShaderiv(shader, GL_COMPILE_STATUS, &ok);
    if (ok == GL_TRUE) return shader;
    GLint logLength = 0;
    glGetShaderiv(shader, GL_INFO_LOG_LENGTH, &logLength);
    std::string log(static_cast<std::size_t>(std::max(logLength, 1)), '\0');
    glGetShaderInfoLog(shader, logLength, nullptr, log.data());
    glDeleteShader(shader);
    throw std::runtime_error("Shader compilation failed: " + log);
}

GLuint Link(std::string_view vertex, std::string_view fragment, const char* label) {
    const GLuint vs = Compile(GL_VERTEX_SHADER, vertex);
    const GLuint fs = Compile(GL_FRAGMENT_SHADER, fragment);
    const GLuint program = glCreateProgram();
    glAttachShader(program, vs);
    glAttachShader(program, fs);
    glLinkProgram(program);
    glDeleteShader(vs);
    glDeleteShader(fs);
    GLint ok = 0;
    glGetProgramiv(program, GL_LINK_STATUS, &ok);
    if (ok != GL_TRUE) {
        GLint logLength = 0;
        glGetProgramiv(program, GL_INFO_LOG_LENGTH, &logLength);
        std::string log(static_cast<std::size_t>(std::max(logLength, 1)), '\0');
        glGetProgramInfoLog(program, logLength, nullptr, log.data());
        glDeleteProgram(program);
        throw std::runtime_error(std::string("Shader link failed (") + label + "): " + log);
    }
    Label(GL_PROGRAM, program, label);
    // Every program samples uTextures[0..7] from texture units 0..7.
    glUseProgram(program);
    if (const GLint textures = glGetUniformLocation(program, "uTextures"); textures >= 0) {
        constexpr std::array<GLint, PaintTextureSlots> Units{0, 1, 2, 3, 4, 5, 6, 7};
        glUniform1iv(textures, static_cast<GLsizei>(Units.size()), Units.data());
    }
    return program;
}

void GLAD_API_PTR DebugMessage(GLenum source, GLenum type, GLuint id, GLenum severity, GLsizei length,
                               const GLchar* message, const void* user) {
    (void)source;
    (void)id;
    (void)length;
    (void)user;
    if (severity == GL_DEBUG_SEVERITY_NOTIFICATION || type == GL_DEBUG_TYPE_PUSH_GROUP ||
        type == GL_DEBUG_TYPE_POP_GROUP)
        return;
    std::fprintf(stderr, "[GL] %s\n", message);
}
} // namespace

GlDevice::GlDevice() {
    const auto program = [](std::string_view vertex, std::string_view fragment, const char* label) {
        Program result;
        result.id = Link(vertex, fragment, label);
        result.viewSize = glGetUniformLocation(result.id, "uViewSize");
        result.sourceTexel = glGetUniformLocation(result.id, "uSourceTexel");
        result.bloomIntensity = glGetUniformLocation(result.id, "uBloomIntensity");
        result.bloomSpread = glGetUniformLocation(result.id, "uBloomSpread");
        result.vignette = glGetUniformLocation(result.id, "uVignette");
        result.scanlines = glGetUniformLocation(result.id, "uScanlines");
        result.scanlineCount = glGetUniformLocation(result.id, "uScanlineCount");
        result.aberration = glGetUniformLocation(result.id, "uAberration");
        return result;
    };
    _programs[static_cast<std::size_t>(GpuProgram::Paint)] = program(PaintVertexShader, PaintFragmentShader, "Paint");
    _programs[static_cast<std::size_t>(GpuProgram::BloomDownsample)] =
        program(FullscreenVertexShader, BloomDownsampleShader, "Bloom downsample");
    _programs[static_cast<std::size_t>(GpuProgram::BloomUpsample)] =
        program(FullscreenVertexShader, BloomUpsampleShader, "Bloom upsample");
    _programs[static_cast<std::size_t>(GpuProgram::Compose)] =
        program(FullscreenVertexShader, ComposeShader, "Compose");

    glGenVertexArrays(1, &_emptyLayout);
    glGenVertexArrays(1, &_paintLayout);
    glBindVertexArray(_paintLayout);
    glGenBuffers(1, &_vertices.buffer);
    glGenBuffers(1, &_indices.buffer);
    glBindBuffer(GL_ARRAY_BUFFER, _vertices.buffer);
    _vertices.capacity = InitialVertexBytes;
    glBufferData(GL_ARRAY_BUFFER, static_cast<GLsizeiptr>(_vertices.capacity), nullptr, GL_STREAM_DRAW);
    glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, _indices.buffer); // Recorded in the vertex array
    _indices.capacity = InitialIndexBytes;
    glBufferData(GL_ELEMENT_ARRAY_BUFFER, static_cast<GLsizeiptr>(_indices.capacity), nullptr, GL_STREAM_DRAW);
    const auto attribute = [](GLuint location, GLint count, GLenum type, bool normalized, std::size_t offset) {
        glEnableVertexAttribArray(location);
        glVertexAttribPointer(location, count, type, normalized ? GL_TRUE : GL_FALSE, Stride,
                              reinterpret_cast<const void*>(offset));
    };
    attribute(0, 2, GL_FLOAT, false, offsetof(PaintVertex, x));
    attribute(1, 2, GL_FLOAT, false, offsetof(PaintVertex, u));
    attribute(2, 4, GL_FLOAT, false, offsetof(PaintVertex, shape0));
    attribute(3, 2, GL_FLOAT, false, offsetof(PaintVertex, style0));
    attribute(4, 4, GL_UNSIGNED_BYTE, true, offsetof(PaintVertex, color));
    attribute(5, 4, GL_UNSIGNED_BYTE, true, offsetof(PaintVertex, color2));
    glEnableVertexAttribArray(6);
    glVertexAttribIPointer(6, 4, GL_UNSIGNED_BYTE, Stride, reinterpret_cast<const void*>(offsetof(PaintVertex, kind)));
    glBindVertexArray(0);
    Label(GL_VERTEX_ARRAY, _paintLayout, "Paint layout");
    Label(GL_BUFFER, _vertices.buffer, "Paint vertices");
    Label(GL_BUFFER, _indices.buffer, "Paint indices");

    glGenTextures(1, &_white);
    glBindTexture(GL_TEXTURE_2D, _white);
    constexpr std::array<std::uint8_t, 4> White{255, 255, 255, 255};
    glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA8, 1, 1, 0, GL_RGBA, GL_UNSIGNED_BYTE, White.data());
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_NEAREST);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_NEAREST);

    _debugGroups = glad_glPushDebugGroup != nullptr;
#ifndef NDEBUG
    if (glad_glDebugMessageCallback) {
        glEnable(GL_DEBUG_OUTPUT);
        glEnable(GL_DEBUG_OUTPUT_SYNCHRONOUS);
        glDebugMessageCallback(DebugMessage, nullptr);
    }
#endif
    ForgetState();
}

GlDevice::~GlDevice() {
    _targets.ForEach([](Target& target) { glDeleteFramebuffers(1, &target.framebuffer); });
    _textures.ForEach([](Texture& texture) { glDeleteTextures(1, &texture.id); });
    for (const Program& program : _programs)
        glDeleteProgram(program.id);
    glDeleteBuffers(1, &_vertices.buffer);
    glDeleteBuffers(1, &_indices.buffer);
    glDeleteVertexArrays(1, &_paintLayout);
    glDeleteVertexArrays(1, &_emptyLayout);
    glDeleteTextures(1, &_white);
#ifndef NDEBUG
    if (glad_glDebugMessageCallback) glDebugMessageCallback(nullptr, nullptr);
#endif
}

GpuTexture GlDevice::AddTexture(const GpuTextureDesc& desc, bool owned) {
    if (desc.width <= 0 || desc.height <= 0) throw std::invalid_argument("GlDevice: texture needs a positive size");
    const Format format = FormatOf(desc.format);
    Texture texture{0, desc, owned};
    texture.desc.label = nullptr;
    glGenTextures(1, &texture.id);
    glActiveTexture(GL_TEXTURE0);
    glBindTexture(GL_TEXTURE_2D, texture.id);
    glTexImage2D(GL_TEXTURE_2D, 0, format.internal, desc.width, desc.height, 0, format.layout, format.type, nullptr);
    const GLint filter = desc.filter == GpuFilter::Linear ? GL_LINEAR : GL_NEAREST;
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, filter);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, filter);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
    Label(GL_TEXTURE, texture.id, desc.label);
    _boundKnown = false;
    return _textures.Add(texture);
}

GpuTexture GlDevice::CreateTexture(const GpuTextureDesc& desc) {
    GpuTexture texture = AddTexture(desc, true);
    if (desc.format == GpuFormat::R8 || desc.format == GpuFormat::Rgba8) {
        // New textures start transparent (GL leaves them undefined).
        const std::vector<std::byte> zero(static_cast<std::size_t>(desc.width) * static_cast<std::size_t>(desc.height) *
                                          FormatOf(desc.format).bytes);
        UpdateTexture(texture, {0, 0, desc.width, desc.height}, zero);
    }
    return texture;
}

void GlDevice::UpdateTexture(GpuTexture handle, PixelRect region, std::span<const std::byte> pixels) {
    const Texture& texture = _textures.Get(handle);
    const Format format = FormatOf(texture.desc.format);
    if (region.x < 0 || region.y < 0 || region.x + region.width > texture.desc.width ||
        region.y + region.height > texture.desc.height ||
        pixels.size() !=
            static_cast<std::size_t>(region.width) * static_cast<std::size_t>(region.height) * format.bytes)
        throw std::invalid_argument("GlDevice: texture update outside the texture or of the wrong size");
    if (region.width == 0 || region.height == 0) return;
    glActiveTexture(GL_TEXTURE0);
    glBindTexture(GL_TEXTURE_2D, texture.id);
    glPixelStorei(GL_UNPACK_ALIGNMENT, 1);
    glTexSubImage2D(GL_TEXTURE_2D, 0, region.x, region.y, region.width, region.height, format.layout, format.type,
                    pixels.data());
    _boundKnown = false;
    _counters.uploadedBytes += pixels.size();
}

void GlDevice::DestroyTexture(GpuTexture handle) {
    if (!_textures.Get(handle).owned) throw std::logic_error("GlDevice: a target's attachment goes with the target");
    const Texture texture = _textures.Remove(handle);
    glDeleteTextures(1, &texture.id);
    _boundKnown = false;
}

GpuTarget GlDevice::CreateTarget(const GpuTargetDesc& desc) {
    Target target;
    target.desc = desc;
    target.desc.label = nullptr;
    const GpuTextureDesc texture{desc.width, desc.height, desc.format, desc.filter, desc.label};
    target.color = AddTexture(texture, false);
    if (desc.emission) target.emission = AddTexture(texture, false);
    glGenFramebuffers(1, &target.framebuffer);
    glBindFramebuffer(GL_FRAMEBUFFER, target.framebuffer);
    glFramebufferTexture2D(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0, GL_TEXTURE_2D, _textures.Get(target.color).id, 0);
    if (desc.emission)
        glFramebufferTexture2D(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT1, GL_TEXTURE_2D, _textures.Get(target.emission).id,
                               0);
    const GLenum status = glCheckFramebufferStatus(GL_FRAMEBUFFER);
    Label(GL_FRAMEBUFFER, target.framebuffer, desc.label);
    _program.reset(); // Framebuffer binding changed; the next pass binds its own
    if (status != GL_FRAMEBUFFER_COMPLETE) {
        glDeleteFramebuffers(1, &target.framebuffer);
        for (const GpuTexture attachment : {target.color, target.emission})
            if (attachment.Valid()) DeleteTexture(_textures.Remove(attachment));
        throw std::runtime_error("GlDevice: render target incomplete (status " + std::to_string(status) + ")");
    }
    return _targets.Add(target);
}

GpuTexture GlDevice::TargetTexture(GpuTarget handle, int attachment) const {
    const Target& target = _targets.Get(handle);
    if (attachment == 1 && !target.emission.Valid()) throw std::logic_error("GlDevice: target has no emission");
    return attachment == 0 ? target.color : target.emission;
}

void GlDevice::DestroyTarget(GpuTarget handle) {
    const Target target = _targets.Remove(handle);
    glDeleteFramebuffers(1, &target.framebuffer);
    for (const GpuTexture attachment : {target.color, target.emission})
        if (attachment.Valid()) DeleteTexture(_textures.Remove(attachment));
    _boundKnown = false;
}

GpuNativeTarget GlDevice::Native(GpuTarget handle) const {
    const Target& target = _targets.Get(handle);
    return {target.framebuffer, _textures.Get(target.color).id, target.desc.width, target.desc.height};
}

void GlDevice::BeginFrame() {
    _counters = {};
    ForgetState();
}

void GlDevice::SetBackBufferSize(int width, int height) {
    _backBufferWidth = std::max(width, 1);
    _backBufferHeight = std::max(height, 1);
}

void GlDevice::EndFrame() { glBindFramebuffer(GL_FRAMEBUFFER, 0); }

std::size_t GlDevice::Write(Stream& stream, const void* data, std::size_t bytes) {
    glBindBuffer(GL_COPY_WRITE_BUFFER, stream.buffer);
    if (stream.offset + bytes > stream.capacity) {
        // Orphan: the driver hands out fresh storage while the GPU may still read the old one.
        while (stream.capacity < bytes)
            stream.capacity *= 2;
        glBufferData(GL_COPY_WRITE_BUFFER, static_cast<GLsizeiptr>(stream.capacity), nullptr, GL_STREAM_DRAW);
        stream.offset = 0;
    }
    const std::size_t at = stream.offset;
    // Only never-used bytes since the last orphaning are written: no synchronisation needed.
    void* target = glMapBufferRange(GL_COPY_WRITE_BUFFER, static_cast<GLintptr>(at), static_cast<GLsizeiptr>(bytes),
                                    GL_MAP_WRITE_BIT | GL_MAP_UNSYNCHRONIZED_BIT | GL_MAP_INVALIDATE_RANGE_BIT);
    if (!target) throw std::runtime_error("GlDevice: could not map the streaming buffer");
    std::memcpy(target, data, bytes);
    glUnmapBuffer(GL_COPY_WRITE_BUFFER);
    stream.offset += bytes;
    _counters.uploadedBytes += bytes;
    return at;
}

GpuGeometry GlDevice::Upload(std::span<const PaintVertex> vertices, std::span<const std::uint32_t> indices) {
    if (vertices.empty() || indices.empty()) return {};
    const std::size_t vertexAt = Write(_vertices, vertices.data(), vertices.size_bytes());
    const std::size_t indexAt = Write(_indices, indices.data(), indices.size_bytes());
    return {static_cast<std::int64_t>(vertexAt / sizeof(PaintVertex)),
            static_cast<std::uint32_t>(indexAt / sizeof(std::uint32_t))};
}

int GlDevice::TargetHeight() const {
    return _pass.target.Valid() ? _targets.Get(_pass.target).desc.height : _backBufferHeight;
}

void GlDevice::BeginPass(const GpuPassDesc& pass) {
    _pass = pass;
    _passEmission = false;
    if (pass.target.Valid()) {
        const Target& target = _targets.Get(pass.target);
        glBindFramebuffer(GL_FRAMEBUFFER, target.framebuffer);
        _passEmission = target.emission.Valid();
        constexpr std::array<GLenum, 2> Buffers{GL_COLOR_ATTACHMENT0, GL_COLOR_ATTACHMENT1};
        glDrawBuffers(_passEmission ? 2 : 1, Buffers.data());
    } else {
        glBindFramebuffer(GL_FRAMEBUFFER, 0);
    }
    const PixelRect& view = pass.viewport;
    glViewport(view.x, TargetHeight() - (view.y + view.height), view.width, view.height);
    glDisable(GL_CULL_FACE);
    glDisable(GL_DEPTH_TEST);
    glDisable(GL_STENCIL_TEST);
    glEnable(GL_BLEND);
    glColorMask(GL_TRUE, GL_TRUE, GL_TRUE, GL_TRUE);
    if (pass.clear) {
        glDisable(GL_SCISSOR_TEST);
        const std::array<GLfloat, 4> color{pass.clear->r, pass.clear->g, pass.clear->b, pass.clear->a};
        glClearBufferfv(GL_COLOR, 0, color.data());
        if (_passEmission) {
            constexpr std::array<GLfloat, 4> None{0.0f, 0.0f, 0.0f, 0.0f};
            glClearBufferfv(GL_COLOR, 1, None.data());
        }
    }
    _program.reset(); // View size uniform is per pass
    _scissor.reset();
    ++_counters.passes;
}

void GlDevice::EndPass() {}

void GlDevice::ApplyBlend(GpuBlend blend) {
    glBlendEquation(GL_FUNC_ADD);
    switch (blend) {
    case GpuBlend::Premultiplied:
        glBlendFuncSeparate(GL_ONE, GL_ONE_MINUS_SRC_ALPHA, GL_ONE, GL_ONE_MINUS_SRC_ALPHA);
        break;
    case GpuBlend::Multiply: glBlendFuncSeparate(GL_DST_COLOR, GL_ONE_MINUS_SRC_ALPHA, GL_ZERO, GL_ONE); break;
    case GpuBlend::Screen  : glBlendFuncSeparate(GL_ONE, GL_ONE_MINUS_SRC_COLOR, GL_ONE, GL_ONE_MINUS_SRC_ALPHA); break;
    case GpuBlend::Replace : glBlendFuncSeparate(GL_ONE, GL_ZERO, GL_ONE, GL_ZERO); break;
    }
}

void GlDevice::SetPipeline(GpuProgram program, GpuBlend blend) {
    if (_program != program) {
        const Program& selected = _programs[static_cast<std::size_t>(program)];
        glUseProgram(selected.id);
        glBindVertexArray(program == GpuProgram::Paint ? _paintLayout : _emptyLayout);
        if (selected.viewSize >= 0) glUniform2f(selected.viewSize, _pass.viewSize.x, _pass.viewSize.y);
        _program = program;
        ++_counters.pipelineChanges;
    }
    if (_blend != blend) {
        ApplyBlend(blend);
        _blend = blend;
    }
}

void GlDevice::SetScissor(const PixelRect* rect) {
    const std::optional<PixelRect> wanted = rect ? std::optional{*rect} : std::nullopt;
    if (_scissor && *_scissor == wanted) return;
    if (rect) {
        glEnable(GL_SCISSOR_TEST);
        glScissor(rect->x, TargetHeight() - (rect->y + rect->height), rect->width, rect->height);
    } else {
        glDisable(GL_SCISSOR_TEST);
    }
    _scissor = wanted;
    ++_counters.scissorChanges;
}

void GlDevice::BindTextures(std::span<const GpuTexture> textures) {
    if (textures.size() > PaintTextureSlots) throw std::invalid_argument("GlDevice: too many textures");
    for (std::size_t slot = 0; slot < PaintTextureSlots; ++slot) {
        const unsigned id = slot < textures.size() ? _textures.Get(textures[slot]).id : _white;
        if (_boundKnown && _bound[slot] == id) continue;
        glActiveTexture(GL_TEXTURE0 + static_cast<GLenum>(slot));
        glBindTexture(GL_TEXTURE_2D, id);
        _bound[slot] = id;
        ++_counters.textureBinds;
    }
    _boundKnown = true;
}

void GlDevice::DrawIndexed(const GpuGeometry& geometry, std::uint32_t firstIndex, std::uint32_t indexCount) {
    if (indexCount == 0) return;
    const std::size_t offset = (static_cast<std::size_t>(geometry.firstIndex) + firstIndex) * sizeof(std::uint32_t);
    glDrawElementsBaseVertex(GL_TRIANGLES, static_cast<GLsizei>(indexCount), GL_UNSIGNED_INT,
                             reinterpret_cast<const void*>(offset), static_cast<GLint>(geometry.baseVertex));
    ++_counters.drawCalls;
}

void GlDevice::DrawFullscreen(const PostUniforms& uniforms) {
    if (!_program) throw std::logic_error("GlDevice: no pipeline");
    const Program& program = _programs[static_cast<std::size_t>(*_program)];
    const auto set = [](GLint location, float value) {
        if (location >= 0) glUniform1f(location, value);
    };
    if (program.sourceTexel >= 0) glUniform2f(program.sourceTexel, uniforms.sourceTexel.x, uniforms.sourceTexel.y);
    set(program.bloomIntensity, uniforms.bloomIntensity);
    set(program.bloomSpread, uniforms.bloomSpread);
    set(program.vignette, uniforms.vignette);
    set(program.scanlines, uniforms.scanlines);
    set(program.scanlineCount, uniforms.scanlineCount);
    set(program.aberration, uniforms.aberration);
    glDrawArrays(GL_TRIANGLES, 0, 3);
    ++_counters.drawCalls;
}

void GlDevice::ForgetState() {
    _program.reset();
    _blend.reset();
    _scissor.reset();
    _boundKnown = false;
}

void GlDevice::ReleaseState() {
    glBindFramebuffer(GL_FRAMEBUFFER, 0);
    glViewport(0, 0, _backBufferWidth, _backBufferHeight);
    glDisable(GL_SCISSOR_TEST);
    glUseProgram(0);
    glBindVertexArray(0);
    glBindBuffer(GL_COPY_WRITE_BUFFER, 0);
    glActiveTexture(GL_TEXTURE0);
    glBlendEquation(GL_FUNC_ADD);
    glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);
    ForgetState();
}

void GlDevice::PushMarker(std::string_view name) {
    if (_debugGroups) glPushDebugGroup(GL_DEBUG_SOURCE_APPLICATION, 0, static_cast<GLsizei>(name.size()), name.data());
}

void GlDevice::PopMarker() {
    if (_debugGroups) glPopDebugGroup();
}

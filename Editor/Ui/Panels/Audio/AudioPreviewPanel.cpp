#include "Editor/Ui/Panels/Audio/AudioPreviewPanel.h"
#include <extras/IconsFontAwesome6.h>
#include <imgui.h>
#include <algorithm>
#include <cctype>
#include <format>
#include <string>
#include <system_error>
#include "Editor/Internals/Session/AudioPreview.h"
#include "Editor/Internals/Session/AudioUsage.h"
#include "ToolUi/ToolTheme.h"
#include "ToolUi/ToolWidgets.h"

namespace {
constexpr double RefreshSeconds = 2.0;
constexpr float MinTimelineHeight = 36;
constexpr float MaxTimelineHeight = 140;
constexpr float FlatBarHeight = 6;
constexpr float VolumeSliderWidth = 120;
constexpr float PlayheadWidth = 2;
constexpr float UnplayedAlpha = 0.45f;
constexpr float PeakAlpha = 0.55f; // The outer, peak part of a column; its RMS core is opaque

ImVec4 WithAlpha(ImVec4 color, float alpha) {
    color.w *= alpha;
    return color;
}

// Loudest bucket value over [first, last) of the summary; at least one bucket.
float MaxOver(const std::vector<float>& values, std::size_t first, std::size_t last) {
    last = std::max(last, first + 1);
    return *std::max_element(values.begin() + static_cast<std::ptrdiff_t>(first),
                             values.begin() + static_cast<std::ptrdiff_t>(std::min(last, values.size())));
}

void DrawWaveform(ImDrawList& draw, const AudioWaveform& waveform, ImVec2 start, ImVec2 end, float played) {
    const float scale = ToolUiScale();
    const float middle = (start.y + end.y) * 0.5f;
    const float half = (end.y - start.y) * 0.5f;
    const auto columns = static_cast<std::size_t>(std::max(1.0f, (end.x - start.x) / scale));
    const auto count = waveform.peaks.size();
    const auto playedColor = ToolToneColor(ToolTone::Success);
    const auto restColor = WithAlpha(ToolPalette::Secondary, UnplayedAlpha);
    draw.AddLine({start.x, middle}, {end.x, middle}, ImGui::GetColorU32(ToolPalette::Separator), scale);
    for (std::size_t column = 0; column < columns; ++column) {
        const std::size_t first = column * count / columns;
        const std::size_t last = (column + 1) * count / columns;
        if (first >= count) break;
        const float x = start.x + static_cast<float>(column) * scale;
        const bool before = x < start.x + (end.x - start.x) * played;
        const float peak = MaxOver(waveform.peaks, first, last) * half;
        const float rms = MaxOver(waveform.rms, first, last) * half;
        const auto color = before ? playedColor : restColor;
        draw.AddLine({x, middle - peak}, {x, middle + peak + scale}, ImGui::GetColorU32(WithAlpha(color, PeakAlpha)),
                     scale);
        draw.AddLine({x, middle - rms}, {x, middle + rms + scale}, ImGui::GetColorU32(color), scale);
    }
}

void DrawFlatBar(ImDrawList& draw, ImVec2 start, ImVec2 end, float played) {
    const float scale = ToolUiScale();
    const float middle = (start.y + end.y) * 0.5f;
    const float half = FlatBarHeight * scale * 0.5f;
    const float radius = half;
    draw.AddRectFilled({start.x, middle - half}, {end.x, middle + half}, ImGui::GetColorU32(ToolPalette::Control),
                       radius);
    if (played > 0)
        draw.AddRectFilled({start.x, middle - half}, {start.x + (end.x - start.x) * played, middle + half},
                           ImGui::GetColorU32(ToolToneColor(ToolTone::Success)), radius);
}

std::string ChannelName(int channels) {
    switch (channels) {
    case 1 : return "Mono";
    case 2 : return "Stereo";
    default: return std::format("{} channels", channels);
    }
}

AudioPreviewRequest Play(const std::filesystem::path& file, const AudioPreviewState& audio) {
    return {AudioPreviewAction::Play, file, audio.loop, audio.volume};
}
} // namespace

void AudioPreviewPanel::Refresh(const std::filesystem::path& file) {
    if (file == _file && ImGui::GetTime() < _nextRefresh) return;
    _file = file;
    _nextRefresh = ImGui::GetTime() + RefreshSeconds;
    std::error_code error;
    _size = std::filesystem::file_size(file, error);
    if (error) _size = 0;
    _previous = NeighborSound(file, -1);
    _next = NeighborSound(file, 1);
}

void AudioPreviewPanel::Header(const AudioPreviewState& audio, const AudioUsage* usage) {
    const auto name = audio.file.filename().string();
    ImGui::TextColored(ToolToneColor(ToolTone::Success), ICON_FA_MUSIC);
    ImGui::SameLine();
    ImGui::PushFont(ToolFont(ToolFontRole::Heading), ToolMetrics::HeadingSize);
    ImGui::TextUnformatted(name.c_str());
    ImGui::PopFont();
    const auto folder = _assetsDirectory.empty()
                            ? audio.file.parent_path()
                            : audio.file.parent_path().lexically_relative(_assetsDirectory.parent_path());
    ImGui::SameLine();
    ImGui::AlignTextToFramePadding();
    ImGui::TextDisabled("%s", folder.generic_string().c_str());
    std::string details = audio.length > 0 ? FormatAudioTime(audio.length) : std::string("--:--");
    auto extension = audio.file.extension().string();
    if (!extension.empty()) extension.erase(0, 1);
    std::ranges::transform(extension, extension.begin(),
                           [](unsigned char c) { return static_cast<char>(std::toupper(c)); });
    details += "  ·  " + extension + "  ·  " + FormatFileSize(_size);
    if (audio.waveform.waveform)
        details += std::format("  ·  {} Hz  ·  {}", audio.waveform.waveform->sampleRate,
                               ChannelName(audio.waveform.waveform->channels));
    ImGui::TextColored(ToolPalette::Secondary, "%s", details.c_str());
    if (usage && usage->reported && !_assetsDirectory.empty()) {
        const auto labels = AudioUseLabels(AudioUsesOf(*usage, audio.file.lexically_relative(_assetsDirectory)));
        if (labels.empty())
            ImGui::TextColored(ToolToneColor(ToolTone::Warning),
                               ICON_FA_CIRCLE_MINUS "  Unused: not in the audio catalog");
        else
            ImGui::TextColored(ToolToneColor(ToolTone::Success), ICON_FA_CIRCLE_CHECK "  %s", labels.c_str());
    }
    if (!audio.error.empty())
        ImGui::TextColored(ToolToneColor(ToolTone::Error), ICON_FA_TRIANGLE_EXCLAMATION "  %s", audio.error.c_str());
}

void AudioPreviewPanel::Timeline(const AudioPreviewState& audio, const EditorUiActions& actions) {
    const float scale = ToolUiScale();
    const float controls = ImGui::GetFrameHeightWithSpacing();
    const float height =
        std::clamp(ImGui::GetContentRegionAvail().y - controls, MinTimelineHeight * scale, MaxTimelineHeight * scale);
    const float width = std::max(1.0f, ImGui::GetContentRegionAvail().x);
    ImGui::InvisibleButton("##timeline", {width, height});
    const auto start = ImGui::GetItemRectMin();
    const auto end = ImGui::GetItemRectMax();
    const float length = audio.length;
    const auto secondsAt = [&](float x) { return std::clamp((x - start.x) / (end.x - start.x), 0.0f, 1.0f) * length; };
    if (length > 0 && ImGui::IsItemActive()) _scrub = secondsAt(ImGui::GetMousePos().x);
    if (_scrub && ImGui::IsItemDeactivated()) {
        if (!audio.playing && !audio.paused && actions.previewAudio) actions.previewAudio(Play(audio.file, audio));
        if (actions.previewAudio)
            actions.previewAudio({AudioPreviewAction::Seek, {}, audio.loop, audio.volume, *_scrub});
        _scrub.reset();
    }
    const float position = _scrub.value_or(audio.position);
    const float played = length > 0 ? std::clamp(position / length, 0.0f, 1.0f) : 0.0f;
    auto& draw = *ImGui::GetWindowDrawList();
    draw.AddRectFilled(start, end, ImGui::GetColorU32(ToolPalette::Content), ToolMetrics::Radius * scale);
    const float inset = ToolMetrics::Gap * scale;
    const ImVec2 innerStart{start.x + inset, start.y + inset};
    const ImVec2 innerEnd{end.x - inset, end.y - inset};
    if (audio.waveform.waveform && !audio.waveform.waveform->peaks.empty())
        DrawWaveform(draw, *audio.waveform.waveform, innerStart, innerEnd, played);
    else
        DrawFlatBar(draw, innerStart, innerEnd, played);
    if (length > 0) {
        const float x = innerStart.x + (innerEnd.x - innerStart.x) * played;
        draw.AddRectFilled({x - PlayheadWidth * scale * 0.5f, start.y}, {x + PlayheadWidth * scale * 0.5f, end.y},
                           ImGui::GetColorU32(ToolPalette::Text));
    }
    if (ImGui::IsItemHovered() && length > 0) {
        const float x = std::clamp(ImGui::GetMousePos().x, innerStart.x, innerEnd.x);
        draw.AddLine({x, start.y}, {x, end.y}, ImGui::GetColorU32(ToolPalette::Secondary), scale);
        ImGui::SetMouseCursor(ImGuiMouseCursor_Hand);
        const float seconds = std::clamp((x - innerStart.x) / (innerEnd.x - innerStart.x), 0.0f, 1.0f) * length;
        ToolTooltip(FormatAudioTime(seconds).c_str());
    }
    if (audio.waveform.state == WaveformState::Failed) ToolTooltip(("No waveform: " + audio.waveform.error).c_str());
}

void AudioPreviewPanel::Transport(const AudioPreviewState& audio, const EditorUiActions& actions) {
    const float scale = ToolUiScale();
    const auto send = [&](const AudioPreviewRequest& request) {
        if (actions.previewAudio) actions.previewAudio(request);
    };
    ImGui::BeginDisabled(!_previous);
    if (ToolIconButton("previous", ICON_FA_BACKWARD_STEP, "Previous sound in the folder") && _previous)
        send(Play(*_previous, audio));
    ImGui::EndDisabled();
    ImGui::SameLine();
    const bool focused = ImGui::IsWindowFocused(ImGuiFocusedFlags_RootAndChildWindows) && !ImGui::GetIO().WantTextInput;
    const bool space = focused && ImGui::IsKeyPressed(ImGuiKey_Space, false);
    if (ToolIconButton("play", audio.playing ? ICON_FA_PAUSE : ICON_FA_PLAY,
                       audio.playing ? "Pause (Space)" : "Play (Space)") ||
        space) {
        if (audio.playing)
            send({AudioPreviewAction::Pause, {}, audio.loop, audio.volume});
        else if (audio.paused)
            send({AudioPreviewAction::Resume, {}, audio.loop, audio.volume});
        else
            send(Play(audio.file, audio));
    }
    ImGui::SameLine();
    ImGui::BeginDisabled(!audio.playing && !audio.paused);
    if (ToolIconButton("stop", ICON_FA_STOP, "Stop")) send({AudioPreviewAction::Stop, {}, audio.loop, audio.volume});
    ImGui::EndDisabled();
    ImGui::SameLine();
    ImGui::BeginDisabled(!_next);
    if (ToolIconButton("next", ICON_FA_FORWARD_STEP, "Next sound in the folder") && _next) send(Play(*_next, audio));
    ImGui::EndDisabled();
    ImGui::SameLine();
    if (ToolIconButton("loop", ICON_FA_REPEAT, "Loop", audio.loop))
        send({AudioPreviewAction::Loop, {}, !audio.loop, audio.volume});
    ImGui::SameLine();
    ImGui::AlignTextToFramePadding();
    const float position = _scrub.value_or(audio.position);
    ImGui::PushFont(ToolFont(ToolFontRole::Technical), 0);
    ImGui::Text("%s / %s", FormatAudioTime(position).c_str(),
                audio.length > 0 ? FormatAudioTime(audio.length).c_str() : "--:--");
    ImGui::PopFont();
    const float volumeWidth = VolumeSliderWidth * scale;
    const float iconWidth = ImGui::GetFontSize() + ImGui::GetStyle().ItemSpacing.x;
    ImGui::SameLine();
    const float right = ImGui::GetContentRegionMax().x - volumeWidth - iconWidth;
    if (ImGui::GetCursorPosX() < right) ImGui::SetCursorPosX(right); // Volume sits at the right edge
    ImGui::AlignTextToFramePadding();
    ImGui::TextDisabled(audio.volume > 0 ? ICON_FA_VOLUME_LOW : ICON_FA_VOLUME_XMARK);
    ImGui::SameLine();
    ImGui::SetNextItemWidth(volumeWidth);
    float volume = audio.volume;
    if (ImGui::SliderFloat("##volume", &volume, 0.0f, 1.0f, "%.2f"))
        send({AudioPreviewAction::Volume, {}, audio.loop, volume});
    ToolTooltip("Volume");
}

void AudioPreviewPanel::Draw(const EditorSnapshot& snapshot, const EditorUiActions& actions) {
    const auto& audio = snapshot.audio;
    if (audio.file.empty()) {
        ToolEmptyState("No sound loaded", "Click a sound in Assets to preview it here.");
        return;
    }
    Refresh(audio.file);
    const float scale = ToolUiScale();
    if (BeginToolPanel("audio-preview", {0, 0}, ToolSurface::Content, 0, ImGuiWindowFlags_NoScrollbar,
                       ToolMetrics::Padding)) {
        ImGui::PushStyleVar(ImGuiStyleVar_ItemSpacing, {ToolMetrics::Gap * scale, ToolMetrics::Gap * scale});
        Header(audio, snapshot.audioUsage);
        Timeline(audio, actions);
        Transport(audio, actions);
        ImGui::PopStyleVar();
    }
    EndToolPanel();
}

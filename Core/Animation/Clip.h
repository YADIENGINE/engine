#pragma once
#include <algorithm>
#include <concepts>
#include <functional>
#include <optional>
#include <span>
#include <stdexcept>
#include <string>
#include <string_view>
#include <type_traits>
#include <utility>
#include <vector>
#include "Animation/AnimationValue.h"
#include "Animation/Timeline.h"
#include "Animation/Track.h"

// One animation of a pose, as constant data: tracks for its fields, and named moments. It holds no clock. An Animator
// plays clips by name; a caller with its own clock can use Sample and ForEachCrossed directly. A second track for one
// field replaces what the first wrote. Invalid data throws in every configuration.
//
//     StepClip<BossPose>{}
//         .Track(&BossPose::arm, {{0, 0.0f}, {20, -1.2f, Curve::InBack}, {30, 0.8f}})
//         .Moment("fire", 24);
template <class PoseType, class Position> class BasicClip {
public:
    template <class T>
    BasicClip& Track(T PoseType::*member, std::type_identity_t<BasicTrack<T, Position>> track) {
        _trackEnd = std::max(_trackEnd, track.Length());
        _tracks.push_back([member, track = std::move(track)](PoseType& pose, double position) {
            pose.*member = track.Sample(position);
        });
        CheckLength();
        return *this;
    }
    // A named point: an Animator meets it once per pass. On a looping clip a moment at the very end is the same instant
    // as position 0 and is never met, so put it at 0.
    BasicClip& Moment(std::string name, Position at) {
        auto moments = _moments;
        const auto after = std::upper_bound(moments.begin(), moments.end(), at,
                                            [](Position position, const auto& moment) { return position < moment.at; });
        moments.insert(after, {std::move(name), at});
        (void)BasicTimeline<Position>(moments); // The same rules as a timeline's moments
        _moments = std::move(moments);
        CheckLength();
        return *this;
    }
    // Starts again from 0 at the end instead of stopping there.
    BasicClip& Looping(bool looping = true) {
        _looping = looping;
        return *this;
    }
    // By default the last key or moment marks the end; this holds the last values for longer.
    BasicClip& Length(Position length) {
        if (!ValidAnimationPosition(length)) throw std::invalid_argument("A clip has an invalid length");
        _length = length;
        CheckLength();
        return *this;
    }

    [[nodiscard]] Position Length() const { return _length ? *_length : ContentEnd(); }
    [[nodiscard]] bool IsLooping() const { return _looping; }
    [[nodiscard]] std::span<const TimelineMoment<Position>> Moments() const { return _moments; }
    [[nodiscard]] PoseType Sample(Position position) const { return SampleAt(static_cast<double>(position)); }
    // The same for a view drawing between steps, with 'alpha' from SimulationRunner::Alpha.
    [[nodiscard]] PoseType Sample(Position position, float alpha) const
        requires std::integral<Position>
    {
        return SampleAt(static_cast<double>(position) - 1.0 + static_cast<double>(alpha));
    }
    // The pose at any position; fields with no track keep their default value. A looping clip wraps.
    [[nodiscard]] PoseType SampleAt(double position) const {
        const auto length = static_cast<double>(Length());
        if (_looping && length > 0.0) position = WrapAnimationPosition(position, length);
        PoseType pose{};
        for (const auto& track : _tracks)
            track(pose, position);
        return pose;
    }
    // Calls fn(name) for each moment in before <= at < after, in order, and at == after too if asked.
    template <class Fn> void ForEachCrossed(Position before, Position after, Fn&& fn, bool includeAfter = false) const {
        for (const auto& moment : _moments)
            if (before <= moment.at && (moment.at < after || (includeAfter && moment.at == after)))
                fn(std::string_view(moment.name));
    }

private:
    [[nodiscard]] Position ContentEnd() const {
        return _moments.empty() ? _trackEnd : std::max(_trackEnd, _moments.back().at);
    }
    void CheckLength() const {
        if (_length && *_length < ContentEnd()) throw std::invalid_argument("A clip's length cuts off its own content");
    }

    std::vector<std::function<void(PoseType&, double)>> _tracks;
    std::vector<TimelineMoment<Position>> _moments;
    std::optional<Position> _length;
    Position _trackEnd{};
    bool _looping = false;
};

// Positions in whole simulation steps: what a simulation uses.
template <class PoseType> using StepClip = BasicClip<PoseType, int>;
// Positions in seconds: for a view or a menu on ordinary time.
template <class PoseType> using SecondsClip = BasicClip<PoseType, float>;

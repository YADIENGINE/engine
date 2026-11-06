#pragma once
#include <algorithm>
#include <concepts>
#include <cstddef>
#include <stdexcept>
#include <string_view>
#include <utility>
#include "Animation/AnimationValue.h"
#include "Animation/ClipSet.h"
#include "Animation/Curve.h"
#include "Animation/Pose.h"

// How Play starts a clip.
template <class Position> struct AnimatorPlay {
    Position blend{};                 // How long to fade from what was showing; 0 cuts
    Curve blendCurve = Curve::Linear; // Shapes the fade, and the fade into 'then'
    std::string_view then{};          // What plays when this clip ends; a looping clip never ends
    Position thenBlend{};             // How long to fade into 'then'
    bool restart = false;             // Play the clip from 0 even if it is already the current one
};

// Plays one clip of a clip set at a time, and fades between them. The game decides what plays: calling Play with the
// current clip every step does nothing, so rules can simply say what should be showing. Moments of the current clip
// reach the callback once per pass; the clip being faded out meets none. An unknown name throws.
//
// Its state is which clips and where on them, so in a simulation it adds no floats to the hash in normal play. Only
// a fade interrupted by another Play keeps a frozen pose, because there is no clip that could give that pose again.
template <AnimationPose PoseType, class Position> class BasicAnimator {
public:
    using Clips = BasicClipSet<PoseType, Position>;
    using Options = AnimatorPlay<Position>;

    BasicAnimator(Clips clips, std::string_view first)
        : _clips(std::move(clips)) {
        _current.clip = _clips.Find(first);
    }

    void Play(std::string_view clip, const Options& options = {}) {
        const int index = _clips.Find(clip);
        const int next = options.then.empty() ? -1 : _clips.Find(options.then);
        if (!ValidAnimationPosition(options.blend) || !ValidAnimationPosition(options.thenBlend))
            throw std::invalid_argument("An animator's fade has an invalid length");
        if (index == _current.clip && !options.restart) return;
        Start(index, options.blend, options.blendCurve);
        _next = next;
        _nextBlend = options.thenBlend;
        _nextCurve = options.blendCurve;
    }

    // One simulation step. Calls onMoment(name) for each moment met, after the animator has moved, so it may call Play.
    // True in the step the current clip ended. Advance first, then read Pose.
    bool Step()
        requires std::integral<Position>
    {
        return Advance(1, [](std::string_view) {});
    }
    template <class Fn>
    bool Step(Fn&& onMoment)
        requires std::integral<Position>
    {
        return Advance(1, onMoment);
    }
    // The same on ordinary time.
    bool Update(Position seconds)
        requires std::floating_point<Position>
    {
        return Advance(seconds, [](std::string_view) {});
    }
    template <class Fn>
    bool Update(Position seconds, Fn&& onMoment)
        requires std::floating_point<Position>
    {
        return Advance(seconds, onMoment);
    }

    [[nodiscard]] PoseType Pose() const { return Evaluate(1.0); }
    // The same for a view drawing between steps, with 'alpha' from SimulationRunner::Alpha.
    [[nodiscard]] PoseType Pose(float alpha) const
        requires std::integral<Position>
    {
        return Evaluate(static_cast<double>(alpha));
    }
    [[nodiscard]] std::string_view Clip() const { return _clips.Name(_current.clip); }
    [[nodiscard]] Position Clock() const { return _current.clock; }
    // The current clip reached its end and stays on its last pose. A looping clip never does.
    [[nodiscard]] bool Finished() const { return _current.done; }
    // A clip ended in the last Step or Update. With 'then', the next clip is already the current one.
    [[nodiscard]] bool JustFinished() const { return _justFinished; }
    [[nodiscard]] bool Blending() const { return _blending; }

    // Everything that decides later steps, for a simulation's State. Writing the clip set is not needed: it is constant.
    template <class Visitor> void State(Visitor& state) const {
        WriteHead(state, _current);
        state.Field("justFinished", _justFinished);
        if (_next >= 0) {
            state.Group("next", [&] {
                state.Field("clip", _clips.Name(_next));
                state.Field("blend", _nextBlend);
                state.Field("curve", _nextCurve);
            });
        }
        if (_blending) {
            state.Group("blend", [&] {
                state.Field("clock", _blendClock);
                state.Field("length", _blendLength);
                state.Field("curve", _blendCurve);
                state.Field("moved", _blendMoved);
                if (_frozen) {
                    state.Group("frozen", [&] { VisitPose(state, _frozenPose); });
                } else {
                    state.Group("from", [&] { WriteHead(state, _from); });
                }
            });
        }
    }

private:
    // Where one clip is playing. 'moved' tells a view that 'clock' just came from one step before.
    struct Head {
        int clip = 0;
        Position clock{};
        bool moved = false;
        bool done = false;
    };

    template <class Visitor> void WriteHead(Visitor& state, const Head& head) const {
        state.Field("clip", _clips.Name(head.clip));
        state.Field("clock", head.clock);
        state.Field("moved", head.moved);
        state.Field("done", head.done);
    }
    void Start(int clip, Position blend, Curve curve) {
        if (_blending && _blendClock >= _blendLength) _blending = false; // A finished fade has nothing to carry over
        if (blend > Position{}) {
            if (_blending) {
                _frozenPose = Pose(); // Read first: it depends on what the fade was fading from
                _frozen = true;
            } else {
                _from = _current;
                _frozen = false;
            }
            _blending = true;
            _blendClock = Position{};
            _blendLength = blend;
            _blendCurve = curve;
            _blendMoved = false;
        } else {
            _blending = false;
            _frozen = false;
        }
        _current = Head{};
        _current.clip = clip;
        _next = -1;
    }
    void Move(Head& head, Position amount) const {
        const auto& clip = _clips.Get(head.clip);
        head.moved = false;
        if (clip.IsLooping()) {
            head.clock = WrapAnimationPosition<Position>(head.clock + amount, clip.Length());
            head.moved = true;
        } else if (!head.done) {
            head.clock = std::min<Position>(head.clock + amount, clip.Length());
            head.moved = true;
            head.done = head.clock >= clip.Length();
        }
    }
    template <class Fn> bool Advance(Position amount, Fn&& onMoment) {
        _justFinished = false;
        if (!(amount > Position{})) return false;
        // A fade stays until the step after it ends, so a view drawing between steps reaches its end smoothly.
        if (_blending && _blendClock >= _blendLength) {
            _blending = false;
            _frozen = false;
        }
        const Head before = _current;
        Move(_current, amount);
        if (_blending) {
            if (!_frozen) Move(_from, amount);
            _blendClock = std::min<Position>(_blendClock + amount, _blendLength);
            _blendMoved = true;
        }
        const bool ended = !before.done && _current.done;
        if (ended) {
            _justFinished = true;
            if (_next >= 0) {
                const Curve curve = _nextCurve;
                const Position blend = _nextBlend;
                Start(_next, blend, curve);
            }
        }
        // Last, working only from 'before': a callback may call Play.
        MeetMoments(_clips.Get(before.clip), before, amount, onMoment);
        return ended;
    }
    template <class Fn>
    static void MeetMoments(const BasicClip<PoseType, Position>& clip, const Head& before, Position amount, Fn& onMoment) {
        const Position length = clip.Length();
        if (clip.IsLooping()) {
            Position at = before.clock;
            Position remaining = amount;
            while (remaining > Position{}) {
                const Position room = length - at;
                if (remaining < room) {
                    clip.ForEachCrossed(at, at + remaining, onMoment);
                    return;
                }
                clip.ForEachCrossed(at, length, onMoment);
                at = Position{};
                remaining -= room;
            }
        } else if (!before.done) {
            const Position after = std::min<Position>(before.clock + amount, length);
            clip.ForEachCrossed(before.clock, after, onMoment, after >= length);
        }
    }
    // Where a head shows at 'alpha' between the last step and this one.
    [[nodiscard]] double Shown(const Head& head, double alpha) const {
        const auto clock = static_cast<double>(head.clock);
        if (!head.moved || alpha >= 1.0) return clock;
        const double shown = clock - 1.0 + alpha;
        const auto& clip = _clips.Get(head.clip);
        return shown < 0.0 && clip.IsLooping() ? shown + static_cast<double>(clip.Length()) : shown;
    }
    [[nodiscard]] PoseType Evaluate(double alpha) const {
        PoseType pose = _clips.Get(_current.clip).SampleAt(Shown(_current, alpha));
        if (!_blending) return pose;
        const auto clock = static_cast<double>(_blendClock);
        const double shown = _blendMoved && alpha < 1.0 ? clock - 1.0 + alpha : clock;
        const auto t = static_cast<float>(shown / static_cast<double>(_blendLength));
        if (t >= 1.0f) return pose;
        const PoseType from = _frozen ? _frozenPose : _clips.Get(_from.clip).SampleAt(Shown(_from, alpha));
        return MixPose(from, pose, Ease(_blendCurve, t));
    }

    Clips _clips;
    Head _current;
    Head _from;
    PoseType _frozenPose{};
    Position _blendClock{};
    Position _blendLength{};
    Position _nextBlend{};
    int _next = -1;
    Curve _blendCurve = Curve::Linear;
    Curve _nextCurve = Curve::Linear;
    bool _blending = false;
    bool _blendMoved = false;
    bool _frozen = false;
    bool _justFinished = false;
};

// Positions in whole simulation steps: what a simulation uses.
template <class PoseType> using StepAnimator = BasicAnimator<PoseType, int>;
// Positions in seconds: for a view or a menu on ordinary time.
template <class PoseType> using SecondsAnimator = BasicAnimator<PoseType, float>;

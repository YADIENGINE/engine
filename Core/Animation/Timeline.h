#pragma once
#include <cmath>
#include <concepts>
#include <cstddef>
#include <format>
#include <initializer_list>
#include <limits>
#include <span>
#include <stdexcept>
#include <string>
#include <string_view>
#include <utility>
#include <vector>

// One named point of a timeline.
template <class Position> struct TimelineMoment {
    std::string name;
    Position at{};
};

// Named moments on a line, as plain constant data: a boss attack, a wave plan, an intro. It holds no clock and changes
// nothing, so the caller owns the time and every question has one answer for one position. Positions start at 0 and
// never go down; the last moment marks the length. Invalid data and unknown names throw in every configuration.
template <class Position> class BasicTimeline {
public:
    using Moment = TimelineMoment<Position>;

    BasicTimeline(std::initializer_list<Moment> moments)
        : BasicTimeline(std::vector<Moment>(moments)) {}
    explicit BasicTimeline(std::vector<Moment> moments)
        : _moments(std::move(moments)) {
        if (_moments.empty()) throw std::invalid_argument("A timeline needs at least one moment");
        Position previous{};
        for (std::size_t i = 0; i < _moments.size(); ++i) {
            const auto& moment = _moments[i];
            if (moment.name.empty()) throw std::invalid_argument("A timeline moment needs a name");
            // Written this way round so that a NaN position fails too.
            if (!(moment.at >= Position{} && moment.at <= std::numeric_limits<Position>::max()))
                throw std::invalid_argument(std::format("Timeline moment \"{}\" has an invalid position", moment.name));
            if (moment.at < previous)
                throw std::invalid_argument(std::format("Timeline moment \"{}\" goes back in time", moment.name));
            for (std::size_t j = 0; j < i; ++j)
                if (_moments[j].name == moment.name)
                    throw std::invalid_argument(std::format("Timeline moment \"{}\" appears twice", moment.name));
            previous = moment.at;
        }
    }

    [[nodiscard]] std::span<const Moment> Moments() const { return _moments; }
    [[nodiscard]] bool Contains(std::string_view name) const {
        for (const auto& moment : _moments)
            if (moment.name == name) return true;
        return false;
    }
    [[nodiscard]] Position PositionOf(std::string_view name) const {
        for (const auto& moment : _moments)
            if (moment.name == name) return moment.at;
        throw std::invalid_argument(std::format("The timeline has no moment \"{}\"", name));
    }
    [[nodiscard]] Position Length() const { return _moments.back().at; }
    [[nodiscard]] bool Finished(Position position) const { return position >= Length(); }

    // Calls fn(name) for each moment in before <= at < after, in order. Advancing a clock from one value to the next
    // meets every moment once and only once, a moment at 0 included: with fixed steps, the step that starts at 30 meets
    // a moment at 30. Going back meets nothing.
    template <class Fn> void ForEachCrossed(Position before, Position after, Fn&& fn) const {
        for (const auto& moment : _moments)
            if (before <= moment.at && moment.at < after) fn(std::string_view(moment.name));
    }
    // The latest moment reached at 'position'; empty before the first. The last of several at one position wins.
    [[nodiscard]] std::string_view PhaseAt(Position position) const {
        std::string_view phase;
        for (const auto& moment : _moments)
            if (moment.at <= position) phase = moment.name;
        return phase;
    }
    // How far 'position' is from moment 'from' to moment 'to', 0..1. Moments at one position jump from 0 to 1 there.
    // Give the result to a curve and Mix: Mix(a, b, EaseOutCubic(timeline.Progress("charge", "fire", age))).
    [[nodiscard]] float Progress(std::string_view from, std::string_view to, Position position) const {
        return Between(from, to, static_cast<double>(position));
    }
    // The same for a view drawing between steps, with 'alpha' from SimulationRunner::Alpha. Like StepSmoothed it runs
    // from the previous step at alpha 0 to the current one at alpha 1, so it stays with values smoothed that way.
    [[nodiscard]] float Progress(std::string_view from, std::string_view to, Position position, float alpha) const
        requires std::integral<Position>
    {
        return Between(from, to, static_cast<double>(position) - 1.0 + static_cast<double>(alpha));
    }
    // Maps an ever-growing clock into one cycle, 0 <= result < Length(), for a timeline that repeats. The last moment
    // is then the same instant as position 0: it is never met, so put what happens there at 0.
    [[nodiscard]] Position Wrap(Position position) const {
        const Position length = Length();
        if (length <= Position{}) return Position{};
        Position wrapped{};
        if constexpr (std::integral<Position>)
            wrapped = position % length;
        else
            wrapped = std::fmod(position, length);
        return wrapped < Position{} ? wrapped + length : wrapped;
    }

private:
    [[nodiscard]] float Between(std::string_view from, std::string_view to, double position) const {
        const auto start = static_cast<double>(PositionOf(from));
        const auto end = static_cast<double>(PositionOf(to));
        if (end < start) throw std::invalid_argument(std::format("Timeline moment \"{}\" is after \"{}\"", from, to));
        if (position >= end) return 1.0f;
        if (position <= start) return 0.0f;
        return static_cast<float>((position - start) / (end - start));
    }

    std::vector<Moment> _moments;
};

// Positions in whole simulation steps: what a simulation uses, because a run's positions are steps.
using StepTimeline = BasicTimeline<int>;
// Positions in seconds: for a view or a menu on ordinary time. A simulation keeps to StepTimeline.
using SecondsTimeline = BasicTimeline<float>;

// Seconds as whole steps at a step rate, rounded to the nearest, for authors who think in seconds:
// {"fire", StepsIn(1.5, 60)}. A negative time gives a negative position, which a timeline refuses.
[[nodiscard]] constexpr int StepsIn(double seconds, int stepsPerSecond) {
    const double steps = seconds * static_cast<double>(stepsPerSecond);
    return static_cast<int>(steps < 0.0 ? steps - 0.5 : steps + 0.5);
}

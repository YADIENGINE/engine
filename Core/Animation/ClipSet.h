#pragma once
#include <cstddef>
#include <format>
#include <initializer_list>
#include <memory>
#include <stdexcept>
#include <string>
#include <string_view>
#include <utility>
#include <vector>
#include "Animation/Clip.h"

template <class PoseType, class Position> struct ClipSetEntry {
    std::string name;
    BasicClip<PoseType, Position> clip;
};

// Named clips that many animators share: one per boss type, not one per boss. It is constant, and a copy shares the same
// data, so a thousand animators stay cheap and none can outlive what it plays. Invalid data throws.
template <class PoseType, class Position> class BasicClipSet {
public:
    using Entry = ClipSetEntry<PoseType, Position>;

    BasicClipSet(std::initializer_list<Entry> entries)
        : BasicClipSet(std::vector<Entry>(entries)) {}
    explicit BasicClipSet(std::vector<Entry> entries) {
        if (entries.empty()) throw std::invalid_argument("A clip set needs at least one clip");
        for (std::size_t i = 0; i < entries.size(); ++i) {
            const auto& entry = entries[i];
            if (entry.name.empty()) throw std::invalid_argument("A clip needs a name");
            for (std::size_t j = 0; j < i; ++j)
                if (entries[j].name == entry.name)
                    throw std::invalid_argument(std::format("Clip \"{}\" appears twice", entry.name));
            if (entry.clip.IsLooping() && !(entry.clip.Length() > Position{}))
                throw std::invalid_argument(std::format("Looping clip \"{}\" has no length", entry.name));
        }
        _entries = std::make_shared<const std::vector<Entry>>(std::move(entries));
    }

    [[nodiscard]] int Count() const { return static_cast<int>(_entries->size()); }
    [[nodiscard]] bool Contains(std::string_view name) const { return Search(name) >= 0; }
    // Throws for an unknown name.
    [[nodiscard]] int Find(std::string_view name) const {
        const int index = Search(name);
        if (index < 0) throw std::invalid_argument(std::format("The clip set has no clip \"{}\"", name));
        return index;
    }
    [[nodiscard]] std::string_view Name(int index) const { return Entry_(index).name; }
    [[nodiscard]] const BasicClip<PoseType, Position>& Get(int index) const { return Entry_(index).clip; }

private:
    [[nodiscard]] int Search(std::string_view name) const {
        for (std::size_t i = 0; i < _entries->size(); ++i)
            if ((*_entries)[i].name == name) return static_cast<int>(i);
        return -1;
    }
    [[nodiscard]] const Entry& Entry_(int index) const { return (*_entries)[static_cast<std::size_t>(index)]; }

    std::shared_ptr<const std::vector<Entry>> _entries;
};

template <class PoseType> using StepClipSet = BasicClipSet<PoseType, int>;
template <class PoseType> using SecondsClipSet = BasicClipSet<PoseType, float>;

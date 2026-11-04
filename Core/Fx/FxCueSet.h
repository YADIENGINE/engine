#pragma once
#include <initializer_list>
#include <memory>
#include <string>
#include <string_view>
#include <vector>
#include "Fx/FxCue.h"

struct FxCueSetEntry {
    std::string name;
    FxCue cue;
    // Seconds of game time before FxStage::Play plays this name again; 0 has no limit. Fifty hits in a frame then look
    // like one, not like fifty flashes.
    float cooldown = 0.0f;
};

// Named cues, the way a ClipSet names clips. It is constant, and a copy shares the same data. Invalid data and unknown
// names throw in every configuration; a name misspelled in a game therefore fails where it plays.
class FxCueSet {
public:
    FxCueSet(std::initializer_list<FxCueSetEntry> entries);
    explicit FxCueSet(std::vector<FxCueSetEntry> entries);

    [[nodiscard]] bool Contains(std::string_view name) const;
    // Throws for an unknown name.
    [[nodiscard]] const FxCue& Get(std::string_view name) const;
    // Throws for an unknown name.
    [[nodiscard]] float Cooldown(std::string_view name) const;
    [[nodiscard]] std::vector<std::string_view> Names() const;
    // A new set with these added; one with a name already here replaces it. The usual way to start from DefaultFxCues.
    [[nodiscard]] FxCueSet With(std::initializer_list<FxCueSetEntry> entries) const;

private:
    [[nodiscard]] const FxCueSetEntry* Search(std::string_view name) const;
    [[nodiscard]] const FxCueSetEntry& Find(std::string_view name) const;
    std::shared_ptr<const std::vector<FxCueSetEntry>> _entries;
};

// The cues every stage starts with: "hit", "explosion" and "pickup". They name no colour and no look: those come from
// FxPlay and the theme. None changes time, so none needs FxStage::SetTime, and none flashes the whole screen, which an
// event that repeats should choose for itself. "hit" has a short cooldown, as hits come in bunches.
[[nodiscard]] const FxCueSet& DefaultFxCues();

#pragma once
#include <string_view>
#include <vector>
#include "Modules/Popup/Popup.h"

class Popup::State {
public:
    struct Entry {
        PopupHandle handle;
        Float2 anchor;
        PopupDraw draw;
        PopupStyle style;
        double age = 0.0;
        double extraHold = 0.0;
        std::optional<PopupFrame> dismissedFrom;
        double dismissedAt = 0.0;
        PopupPulse pulse;
        double pulseAt = -1.0;
    };

    explicit State(const PopupSettings& options);
    // False (logic_error when strict) while Draw runs: nothing may change the entries under it.
    [[nodiscard]] bool CanMutate() const;
    // Throws invalid_argument when strict; otherwise records the rejection and the caller skips its work.
    void Reject(std::string_view message) const;
    [[nodiscard]] Entry* Find(PopupHandle handle);
    [[nodiscard]] const Entry* Find(PopupHandle handle) const;
    [[nodiscard]] static double Lifetime(const Entry& entry);
    [[nodiscard]] PopupFrame Sample(const Entry& entry) const;

    PopupSettings settings;
    std::vector<Entry> entries;
    bool drawing = false;
    mutable std::size_t rejected = 0;
    mutable std::string firstRejection;

private:
    [[nodiscard]] float Curve(const PopupTransition& transition, float progress) const;
};

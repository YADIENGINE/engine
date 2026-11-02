#pragma once
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
        std::uint64_t order = 0;
    };

    explicit State(const PopupSettings& options);
    void RequireMutable() const;
    [[nodiscard]] Entry* Find(PopupHandle handle);
    [[nodiscard]] const Entry* Find(PopupHandle handle) const;
    [[nodiscard]] static double Lifetime(const Entry& entry);
    [[nodiscard]] static PopupFrame Sample(const Entry& entry);
    static void ValidatePosition(Float2 position);

    PopupSettings settings;
    std::shared_ptr<const std::byte> owner = std::make_shared<const std::byte>();
    std::vector<Entry> entries;
    std::uint64_t nextSequence = 1;
    bool drawing = false;
};

#pragma once
#include <random>
#include <string_view>
#include <vector>
#include "Graphics/Text/TextBlock.h"
#include "Modules/NumberPopup/NumberPopup.h"

class NumberPopup::State {
public:
    struct Entry {
        PopupHandle popup;
        std::optional<NumberPopupValue> value; // Empty: a label
        std::string text;
        NumberPopupStyle style;
        NumberPopupMergeKey mergeKey;
        double changedAt = 0.0; // Clock; ahead of it while delayed, so the merge window starts on activation
        float intensity = 1.0f;
        float flashAt = 0.0f;
        float flashIntensity = 1.0f;
        bool flashes = true;
        bool pendingBurst = true;
        mutable std::optional<TextBlock> layout;
    };

    explicit State(const NumberPopupSettings& settings);
    // False (logic_error when strict) while Draw runs.
    [[nodiscard]] bool CanMutate() const;
    // Throws invalid_argument when strict; otherwise records the rejection and the caller skips its work.
    void Reject(std::string_view message) const;
    // Drops entries whose Popup is gone: the popup's callback is the only other owner of an entry.
    void Prune();
    [[nodiscard]] std::shared_ptr<Entry> Find(PopupHandle handle) const;
    // Starts an entry already holding its value/text; empty handle on capacity rejection.
    [[nodiscard]] PopupHandle Launch(Float2 position, const std::shared_ptr<Entry>& entry,
                                     const NumberPopupEmit& options);
    void Burst(const Entry& entry, Float2 position, float intensity);
    bool Accumulate(Entry& entry, NumberPopupValue delta, float intensity);
    bool Emphasize(Entry& entry, float intensity);
    [[nodiscard]] static bool ValidIntensity(float intensity);
    [[nodiscard]] static bool ValidFormat(const NumberPopupFormat& format);
    [[nodiscard]] static bool ValidStyle(const NumberPopupStyle& style);
    [[nodiscard]] static bool ValidValue(const NumberPopupValue& value);
    [[nodiscard]] static NumberPopupValue Sum(const NumberPopupValue& left, const NumberPopupValue& right);
    [[nodiscard]] static std::string FormatValue(const NumberPopupValue& value, const NumberPopupFormat& format);
    static void DrawEntry(Painter& paint, const PopupFrame& frame, const Entry& entry);

    Popup popups;
    ParticleSystem2D particles;
    std::mt19937 rng;
    FxQuality quality;
    std::vector<std::shared_ptr<Entry>> entries;
    double clock = 0.0;
    bool drawing = false;
    bool strict;
    mutable std::size_t rejected = 0;
    mutable std::string firstRejection;
};

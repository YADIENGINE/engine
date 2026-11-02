#pragma once
#include <random>
#include <vector>
#include "Fx/ParticleSystem2D.h"
#include "Graphics/Text/TextBlock.h"
#include "Modules/NumberPopup/NumberPopup.h"

class NumberPopup::State {
public:
    struct Entry {
        PopupHandle popup;
        NumberPopupValue value;
        std::string text;
        NumberPopupStyle style;
        NumberPopupMergeKey mergeKey;
        double changedAt = 0.0;
        float intensity = 1.0f;
        float flashAt = 0.0f;
        float flashIntensity = 1.0f;
        bool flashes = true;
        bool pendingBurst = true;
        mutable std::optional<TextBlock> layout;
    };

    explicit State(const NumberPopupSettings& settings);
    void RequireMutable() const;
    void Prune();
    [[nodiscard]] std::shared_ptr<Entry> Find(PopupHandle handle) const;
    void Burst(const Entry& entry, Float2 position, float intensity);
    bool Accumulate(Entry& entry, NumberPopupValue delta, float intensity);
    bool Emphasize(Entry& entry, float intensity);
    static void ValidateIntensity(float intensity);
    static void ValidateFormat(const NumberPopupFormat& format);
    static void ValidateStyle(const NumberPopupStyle& style);
    static void ValidateValue(const NumberPopupValue& value);
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
};

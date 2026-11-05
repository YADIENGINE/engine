#pragma once
#include <concepts>
#include <initializer_list>
#include <optional>
#include <span>
#include <string>
#include <utility>
#include <variant>
#include <vector>
#include "Fx/Particles/ParticleRecipes.h"
#include "Fx/World/FlashLights.h"
#include "Utilities/Color4.h"

// What a cue can do. Each is one FxStage call, as plain data: where it happens, its colour and its direction come from
// FxPlay, its look from the stage's style. An empty text takes FxPlay::text.
struct FxBurst {
    ParticleRecipe recipe = ParticleRecipe::Soft;
    int count = 0;
};
struct FxParticles { // A burst of the game's own
    ParticleBurst burst{};
    int count = 0;
};
struct FxTrail {};
struct FxDash {};
struct FxSteam {};
struct FxFloatText {
    std::string text{};
};
struct FxPopText {
    float size = 0.0f;
    std::string text{};
};
struct FxRing {};
struct FxShockwave {
    float reach = 0.0f; // 0: the whole area
};
struct FxFlash {};
// A short light, at FxPlay::at unless it has no radius: then the whole scene. It reaches the screen through
// FxStage::LightFlashes.
using FxLightFlash = LightFlashStyle;
struct FxBanner {
    std::string text{};
};
struct FxToast {
    std::string text{};
};
struct FxShake { // Adds to the shake; see CameraEffects::Shake
    float amount = 0.0f;
};
struct FxPunch { // Zooms in on FxPlay::at
    float amount = 0.0f;
};
struct FxKick { // Pushes the camera along FxPlay::facing
    float strength = 1.0f;
};
struct FxGlitch {
    float seconds = 0.0f;
};
// These two change time, so the stage needs FxStage::SetTime.
struct FxHitstop {
    float seconds = 0.0f;
};
struct FxSlowMotion { // Time and its look together
    float seconds = 0.0f;
    float startScale = 0.25f;
    float endScale = 1.0f;
};

using FxAction = std::variant<FxBurst, FxParticles, FxTrail, FxDash, FxSteam, FxFloatText, FxPopText, FxRing,
                              FxShockwave, FxFlash, FxLightFlash, FxBanner, FxToast, FxShake, FxPunch, FxKick, FxGlitch,
                              FxHitstop, FxSlowMotion>;

template <class T, class Variant> inline constexpr bool IsFxActionOf = false;
template <class T, class... Actions>
inline constexpr bool IsFxActionOf<T, std::variant<Actions...>> = (std::same_as<T, Actions> || ...);
template <class T>
concept FxActionType = IsFxActionOf<T, FxAction>;

// One action of a cue: when after the start, and a colour of its own if FxPlay::color is not wanted.
struct FxCueEntry {
    float at = 0.0f;
    FxAction action;
    std::optional<Color4> color;

    template <FxActionType Action>
    FxCueEntry(Action actionNow, std::optional<Color4> ownColor = std::nullopt)
        : action(std::move(actionNow))
        , color(ownColor) {}
    template <FxActionType Action>
    FxCueEntry(float atSeconds, Action later, std::optional<Color4> ownColor = std::nullopt)
        : at(atSeconds)
        , action(std::move(later))
        , color(ownColor) {}
};

// Several effects that belong together, as constant data: what an event looks like, not when it happens. Invalid data
// throws in every configuration, when the cue is made rather than when it first plays.
//
//     FxCue hit{FxBurst{ParticleRecipe::Debris, 24}, FxShake{0.5f}, {FxFlash{}, White}, {0.08f, FxShockwave{}}};
class FxCue {
public:
    FxCue(std::initializer_list<FxCueEntry> entries);
    explicit FxCue(std::vector<FxCueEntry> entries);

    // In order of time.
    [[nodiscard]] std::span<const FxCueEntry> Entries() const { return _entries; }
    [[nodiscard]] bool UsesTime() const { return _usesTime; }

private:
    std::vector<FxCueEntry> _entries;
    bool _usesTime = false;
};

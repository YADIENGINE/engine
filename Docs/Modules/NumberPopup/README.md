# NumberPopup

Opt-in numeric feedback with exact integral accumulation, configurable labels, animated launches and local particles.
Link `EngineNumberPopup`; include `Modules/NumberPopup/NumberPopup.h`. It publicly depends on `EnginePopup` for
animation, handles and drawing. EngineHeadless supplies text layout, particles and math transitively. It has no
Localization, theme, audio or runtime dependency.

## Usage

Own one instance in a View/Scene. Styles are reusable values; emissions snapshot them.

```cpp
NumberPopup numbers;

numbers.Emit(hitPosition, 120);                                   // Default style
numbers.Emit(hitPosition, 850, NumberPopupPreset::Heavy,          // A preset, with per-emission choices
             {.intensity = 1.4f, .color = Color4{255, 190, 60, 255}});
numbers.EmitLabel(hitPosition, "CRIT!", NumberPopupPreset::Burst); // Text with the same animation and juice

auto impact = NumberPopup::Preset(NumberPopupPreset::Burst);      // An edited, reusable style
impact.text.outline.width = 2.0f;

const auto group = NumberPopupMergeKey::Of(DamageKind::Poison, targetId); // Caller-defined stream
auto handle = numbers.Emit(hitPosition, 120, impact, {.mergeKey = group});
numbers.Emit(hitPosition, 30, impact, {.mergeKey = group}); // 150, with a fresh pulse
numbers.Add(handle, 10);                                    // 160

// Once per frame in the owner's update/draw:
numbers.Update(dt);
numbers.Draw(paint);
// On reset:
numbers.Clear();
```

[PopupEffects](../../../AgentDocs/Recipes/PopupEffects.md) is a compiling consumer example. Draw requires the game's
font through `Painter::Font()`. The caller chooses the time domain, camera, clipping and layer; local particles draw
behind the numbers. Number entries may expire slightly before their final particles.

## Public API

| Operation | Contract |
| --- | --- |
| `NumberPopup(settings = {})` | Owns a private Popup and particle system. Default capacities: 128 numbers, 1024 particles. |
| `Preset(preset)` | Editable Pop (the default style), Float, Burst or Heavy visual style. |
| `Format(value, format = {})` | Formats a label without emitting/drawing. Decimals are clamped to 0..9 here. |
| `Emit(position, value, style or preset = {}, options = {})` | Creates or explicitly groups numeric feedback. Empty handle on capacity rejection or skipped invalid input. |
| `EmitLabel(position, label, style or preset = {}, options = {})` | The same animation and juice for text. No value, `format` or merging; `Add` returns false. |
| `Add(handle, delta)` | Adds a value, refreshes the label, pulses and extends hold within the lifetime cap. |
| `Pulse(handle, intensity = 1)` | Local pulse/flash/smaller particle burst without changing the value or lifetime. |
| `SetAnchor(handle, position)` | Translates the live trajectory; already emitted particles keep their own flight. |
| `Dismiss(handle)` / `Cancel(handle)` | Animated exit / immediate number removal. Existing particles finish independently. |
| `SetQuality(quality)` | Applies FxQuality particle percentage and flash choice, including flash choice for existing entries. |
| `Update(deltaSeconds)` | Updates both owned systems. Nonpositive/nonfinite deltas do nothing. |
| `Draw(paint) const` | Draws both systems without advancing time or random state. |
| `Clear()` | Removes all numbers/particles, releases callbacks, invalidates handles and resets the local clock. |
| `IsAlive(handle)` / `Value(handle)` / `Frame(handle)` | Liveness, optional numeric value (empty for labels), optional owned pose snapshot. |
| `ActiveCount()` / `ParticleCount()` | Number entries including delayed entries / live residual particles. |
| `RejectedCount()` / `FirstRejection()` | Calls skipped for invalid input (non-strict only) and the first reason. |

All handle mutations return false for expired/foreign handles. `Add` and `Pulse` also return false once Exit starts.
Dismiss does not restart an existing exit. Capacity policy is `PopupOverflow::ReplaceOldest` by default, or RejectNew;
capacity zero disables emission. A valid merge can still succeed at capacity because it uses an existing entry.

`NumberPopupEmit` holds the per-emission choices: `intensity`, `mergeKey` and a `color` that replaces the style's text
color for that entry, so one style serves every color.

## Styles and juice

- `popup`: all [Popup](../Popup/README.md#animation-and-drawing) timing, motion and layer settings.
- `text`: the existing TextStyle (color, size, width fitting, outline, shadow, glow, emission and spacing). The entire
  laid-out label is centered on the popup origin; alignment controls lines within it. Layout is cached per entry and
  refreshed on value/font changes. No Painter or FontFace is retained.
- `format`: sign, decimals, compact notation, trailing-zero trimming, prefix/suffix or a custom complete-label formatter.
- `juice`: launch-velocity scatter, rotation spread, short color flash, the reusable pulse, and particles as the
  engine's own `ParticleEmission` and `ParticleAppearance` plus `particleCount`. Particles take the text color unless
  `particlesUseTextColor` is false. Angles are clockwise radians. The popup motion also offers damped wobble and drag.
- `merge`: sliding merge window, hold gained per addition, maximum extended lifetime and `followAnchor`.

Intensity is finite and in 0..4. It scales launch variation, entrance squash/rotation, wobble, pulse, flash strength
and particle count/speed. Zero retains readable numeric feedback and its base flight/fade. Intensity never changes
the numeric value or automatically adds camera shake, hitstop or sound. Float has no random scatter/flash/particles;
Pop is a small entrance, Burst adds a ballistic launch and stars, Heavy has stronger squash and a larger local burst.
FxQuality fields for camera effects do not affect this module. Painter controls glow/render quality as usual.

Randomness is sampled on emission, with an optional settings seed for replay within the same toolchain. No standard
library RNG sequence is promised. Clear does not reseed the generators. Delayed entries emit their initial particles
when an Update first activates them; a step that skips the entire entry does not emit a stale burst.

## Values and formatting

`NumberPopupValue` is `variant<int64_t, double>`: normal signed integer inputs select the integral alternative,
float/double inputs select double. Pure integral accumulation and unabridged integral labels retain all 64 bits.
Accumulation saturates at the type's limits instead of failing. Mixed integral/floating additions use double and its
precision. Unsigned inputs must fit the signed alternative; explicitly convert larger domains according to the
application's rules.

Default formatting is fixed zero decimal places, negative sign only. Decimal places are 0..9. Compact labels use
K/M/B/T, promoting a unit when rounding would produce 1000 of the previous unit. Rounding follows the standard
library's fixed formatting; negative rounded zero is normalized to zero. Built-in formatting is locale independent.
Sign policy applies to the number before prefix/suffix are attached. A custom formatter returns the entire owned
label, bypassing built-in formatting; a caller can capture its localization service if needed. It is invoked on
emission and successful value changes, never during Draw. Formatter errors propagate before a value is changed.

## Merging and lifecycle

Key zero means independent emission. A nonzero key groups compatible events chosen by the application;
`NumberPopupMergeKey::Of(parts...)` builds one from integers and enums. Position alone never merges entries. Within the
first entry's configured window, equal keys with compatible signs add numerically, retaining that entry's style and
original launch, and its anchor unless `merge.followAnchor` moves it to the incoming position. Opposite nonzero signs
stay separate. The incoming style can disable auto-merge with a zero window; otherwise the first entry's merge
configuration governs the group. A delayed entry's window starts when it activates, so hits merge while it waits.

Each accepted addition refreshes the sliding window, replaces the short pulse and adds `merge.extendSeconds` of hold
time, capped by maxLifetimeSeconds. This cap never shortens a longer initial animation. Exit is never rewound or
extended, so continuous events cannot keep an entry alive indefinitely. Explicit Add may cross signs and uses the
same extension/pulse behavior, also when the window is zero. Pulse alone does not refresh the merge window.

Instances are noncopyable/nonmovable and main-thread only. Captured formatter resources must outlive the entry;
formatters/easing must not reenter the owner. No game, core FxStage or theme is automatically modified by linking this
module.

Errors follow [Popup](../Popup/README.md#ownership-and-failures): `NumberPopupSettings::strict` defaults to on in Debug
and off in Release. Invalid positions, values, deltas, styles and intensities throw when strict; otherwise the call is
skipped and counted. This applies even when the handle is stale. Nonpositive or nonfinite `Update` deltas are ignored
in both modes.

# Localization

Source/API include root: `Engine/Core/Modules/Localization/`. Documentation and plans live here;
`Internals/` and `Tests/` paths below refer to the source module directory.

Immutable, headless translation catalogues with language selection, fallback, placeholders and number formatting.
Link `EngineLocalization` and include only `Modules/Localization/Localization.hpp`.
Dependencies: C++20 standard library only; no Game, raylib, IO or other module dependencies.

## Public API

`Localization` is the single service entry point. `Language` is the module-owned persisted value contract:
`English = 0`, `Turkish = 1`, `Welsh = 2`. These values must never be renumbered.
Adding another language requires extending the enum, validation and private language presentation metadata.

- `LanguageDefinition { id, enabled, required }` declares authored languages. Array order determines translation
  columns and selection order. The module supplies native names (`English`, `Türkçe`, `Cymraeg`) and percentage style.
- `Settings { languages, fallbackLanguage }` selects the authored language metadata and fallback.
- `Row<Key, LanguageCount> { id, values }` holds one caller-defined key and its null-terminated translations.
  Keys must be contiguous zero-based indices; row order is free.
- `LanguagesValid`, `RowsValid<TextCount>` and `BuildTable<TextCount>` are static constexpr authoring operations.
  They reject unknown/duplicate language IDs, absent fallback, no enabled language, mismatched columns, missing,
  duplicate or out-of-range keys, null strings, and empty required/fallback translations. Optional translations
  may be empty. Invalid `BuildTable` input throws `std::invalid_argument`, failing constant evaluation when constexpr.
- `Localization(table, settings)` borrows immutable row-major translations, metadata and strings. All must outlive
  the catalogue. Construction rejects malformed metadata, dimensions and contents with `std::invalid_argument`.
- `AvailableLanguages()` returns a borrowed span of enabled `{ id, nativeName }` options in authored order,
  valid until catalogue destruction. It contains no unregistered or disabled language.
- `NormalizeLanguage(storedInt64)` retains an enabled registered language; otherwise selects the first enabled one.
- `StepLanguage(language, direction)` normalizes then cycles enabled options. Negative goes backward;
  zero/positive goes forward. One option cycles to itself.
- `Lookup(language, key)` accepts caller enum keys directly. Disabled registered languages can be read;
  an empty translation falls back per key. Unregistered/unknown languages use the fallback column.
  Returned pointers borrow authored strings. Invalid keys assert in Debug and return `""` in Release.
- `Format(language, key, arguments)` returns an owned string, replacing `{name}` with each `Argument { name, value }`
  in argument order. All occurrences are replaced; unknown placeholders remain unchanged. There is no escaping,
  pluralization or recursive expansion within one argument; a later argument can replace earlier inserted text.
- Static `FormatShortNumber(value)` uses up to two decimal places without trailing zeroes; `FormatMultiplier(value)`
  rounds to one decimal place; `FormatSignedPercent(fraction, language)` rounds to an integer percentage with sign.
  Turkish places `%` before the digits. Unknown languages use English percentage placement.
  These functions preserve the existing finite gameplay-number formatting; they are not general locale formatters.

The catalogue has no active language, registration mutators or persistence. Construction is the registration step.
Const operations may run concurrently while borrowed inputs remain immutable; callers own synchronization of
destruction. Copy and move are disabled. Display/UI, saved language values, key definitions, gameplay-to-key mappings
and language-specific tip pools belong to the consumer.

## Short authoring API

`Loc` is a type alias for `Localization`, declared by the module header. It adds no service or state.

`Loc::Texts<Key>(languages, [](auto& text) { ... })` infers language columns from the metadata array.
Call `text(key, translations...)` once per row. The callback must be captureless (`[]`) and constant-evaluable:
the module invokes a default-constructed callback to count rows at compile time, then fills an exactly-sized
`std::array` with the supplied callback. Keep emission deterministic and independent of the emitter type.
Empty groups, column mismatches and capturing callbacks fail compilation; a changed emitted row count throws
`std::invalid_argument` (and fails constant evaluation for constexpr content).
The callback can use local constants, loops and `using enum Key`. It must not perform IO or mutate external state.
Strings still borrow static storage. `RowsValid/BuildTable` continue to validate complete catalogue content.

`Loc::Bindings<EntityId, Key>([](auto& bind) { ... })` allocates a fixed array using `EntityId::Count`.
Call `bind(id, nameKey[, descriptionKey])` for each entity in order. Missing or excess rows and invalid
bindings throw `std::invalid_argument`, failing constant evaluation for constexpr content.
The callback emitters only construct values; they cannot access a catalogue or any backend.

Place `using enum Key` inside each callback to shorten keys without exporting enum names from headers.
Keep entity IDs qualified: importing two enums that both contain `Count` into one scope would conflict.
The lower-level row and variadic factories below remain available.

`Name(language, bindings, id)` and `Desc(language, bindings, id)` resolve and translate fields without a
`TextField` argument. `NameKey(bindings, id)` and `DescKey(bindings, id)` return the corresponding keys.
`FormatDesc(language, bindings, id, arguments)` formats a mapped description. These use the same fallback,
ownership and failure behavior as `Lookup`, `KeyFor` and `Format`.

## Authoring and entity bindings

Static `Text(key, translations...)` infers the key type and number of language columns.
`Texts(first, rest...)` creates a fixed-size group; all rows must have the same key type and column count.
`Group(array)` borrows a group as a read-only span for `RowsValid` and `BuildTable`.
Column count is checked against the authored languages when building the catalogue.

Static `Bind(entityId, nameKey[, descriptionKey])` creates a public `Binding<EntityId, Key>` value.
`Bindings(first, rest...)` builds a fixed-size entity table. `BindingsValid(array)` checks that entity IDs
are contiguous, zero-based, in enum order and cover `EntityId::Count` exactly. Name and optional description
keys must be below `Key::Count`; both enums must define `Count`. Duplicate, missing, unordered or invalid
IDs/keys are rejected with `std::invalid_argument` by `Bindings`, failing constant evaluation for constexpr
tables. Groups and bindings must contain at least one row.

`BindingSet<EntityId, Key> { rows }` is a borrowed public value view of a validated bindings array.
The immutable array must outlive the view and every call using it. It owns no storage or backend;
keep storage static when exporting a view between translation units.
`TextField::Name` and `TextField::Description` select the corresponding key.
Static `KeyFor(view, entityId, field)` returns a caller-owned key; the catalogue overload
`Lookup(language, view, entityId, field)` resolves and translates it directly.
An out-of-range or mismatched entity ID throws `std::out_of_range`; an absent description or invalid
field throws `std::invalid_argument`. Missing descriptions never silently reuse names.
Use `KeyFor` with `Format` when the mapped text contains placeholders.
No game entity types, active selections or binding registry are stored in the module.

## Example

```cpp
#include "Modules/Localization/Localization.hpp"

enum class Label { Play, Score, Count };
constexpr std::array Languages{Localization::LanguageDefinition{Language::English, true, true},
                               Localization::LanguageDefinition{Language::Turkish, true, true}};
constexpr auto Rows = Loc::Texts<Label>(Languages, [](auto& text) {
    using enum Label;
    text(Score, "Score: {score}", "Puan: {score}");
    text(Play, "Play", "Oyna");
});
constexpr std::array Groups{Localization::Group(Rows)};
constexpr auto Count = static_cast<std::size_t>(Label::Count);
static_assert(Localization::RowsValid<Count>(Groups, Languages, Language::English));
constexpr auto Table = Localization::BuildTable<Count>(Groups, Languages, Language::English);

enum class Button { Start, Count };
constexpr auto ButtonBindings = Loc::Bindings<Button, Label>([](auto& bind) {
    using enum Label;
    bind(Button::Start, Play);
});
constexpr Localization::BindingSet<Button, Label> ButtonKeys{ButtonBindings};

// Inside application startup; authored arrays above have static storage duration:
// const Localization catalog(Table, {Languages, Language::English});
// const char* play = catalog.Lookup(Language::Turkish, Label::Play);
// const auto score = catalog.Format(Language::Turkish, Label::Score, {{"score", "15"}});
// const char* start = catalog.Name(Language::Turkish, ButtonKeys, Button::Start);
// for (const auto& option : catalog.AvailableLanguages()) { /* option.id, option.nativeName */ }
```

## Tests

`Tests/` is discovered automatically as `EngineLocalizationTests`, linking only `EngineLocalization`
and Engine test support. Cases use `[localization]`; Game tests cover only authored game content/mappings.

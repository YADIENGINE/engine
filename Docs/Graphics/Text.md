# Text and fonts

`Game::Text()` ortak `TextRenderer` servisidir. Boş `GameSettings::font` raylib varsayılan fontunu kullanır.
Özel font asset köküne göre yoldur; başlangıçta doğrulanması için `settings.assets` listesine de ekleyin.
İstenen Unicode glyph'lerini `fontCodepoints` ile verin; sadece çeviri metnini eklemek font kapsamını genişletmez.

Önceden yüklenen `fontAtlasSizes` varsayılanı 40, 48, 56, 64, 80, 96, 120, 144'tür.
Yazı ekranda kapladığı piksel boyuna göre uygun atlastan çizilir; en küçük atlastan küçük boylar
ilk kullanımda rasterize edilir. Ölçümler pencere boyutundan bağımsızdır.
Font seçimi veya metin değişirse saklanan layout'u yeniden oluşturun.

## Text layout

`TextRenderer::Layout(label, TextLayoutOptions)` wraps text at word boundaries using the renderer's
raylib font. `fontSize` must be positive; `maxWidth = 0` leaves width unrestricted and `maxLines = 0`
allows unlimited lines. Repeated spaces collapse; LF/CRLF breaks and blank lines are preserved. Words
remain intact, including UTF-8 words. `TextWidthOverflow::Allow` retains the nominal size even for
oversized words; `ShrinkToFit` fits each completed line using the same calculation as `FittedSize`.
Fitting stops at font size one, which can still exceed exceptionally narrow bounds.

`TextLineOverflow::Discard` drops content beyond the line limit. `AppendToLastLine` joins the remainder
onto the last allowed line, including text after subsequent hard breaks; combine it with `ShrinkToFit`
to preserve all text in a limited number of rows. Defaults are unlimited lines, unrestricted width,
and `Allow`/`Discard` overflow policies.

```cpp
const auto layout = text.Layout(label, {.fontSize = 20, .maxWidth = bounds.width, .maxLines = 3,
                                       .widthOverflow = TextWidthOverflow::ShrinkToFit,
                                       .lineOverflow = TextLineOverflow::AppendToLastLine});
text.DrawMultiline(layout, bounds, WHITE, {.alignment = TextAlignment::Center, .lineSpacing = 4});
```

`DrawMultiline` starts at `bounds.y`, aligns each line left or centered within `bounds.width`, and
advances by the nominal font size plus nonnegative `lineSpacing`. Smaller fitted text is vertically
centered within its nominal row, matching `DrawCentered`. The caller controls clipping; `bounds.height`
does not truncate text. Colors and spacing are drawing choices, separate from line breaking.

`Graphics/Draw/TextLayout.h` also exposes `LayoutText` and `FittedTextSize` to headless consumers through
a `TextMeasure` callback taking a null-terminated string and font size and returning integer width.
Both calculations use the production renderer's measurements at runtime. A `TextLayout` owns its
strings and resolved widths/font sizes, retaining no callback or font reference. Rebuild it if the
font or input/options change; there is no automatic cache. Tests may supply deterministic measurements.


## Yerelleştirme

Use the standalone [Localization module](../Modules/Localization/README.md), linked as `EngineLocalization`.
Its sole entry point is `Modules/Localization/Localization.hpp`. The module owns persisted `Language`
identities, native display names and percentage style. The consumer supplies authored language declarations,
fallback and constexpr translation rows, then constructs an immutable `Localization` catalogue.

Call `Lookup(language, key)` or `Format(language, key, arguments)` with consumer enum keys directly.
`AvailableLanguages()` supplies enabled options in authored order; `NormalizeLanguage` and `StepLanguage`
handle selection without storing the active language. Game content, tip pools, gameplay-to-key mappings
and persistence remain outside the module. See its README for ownership, validation and a complete example.

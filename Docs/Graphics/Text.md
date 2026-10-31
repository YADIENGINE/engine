# Text and fonts

Text uses `FontFace` and `Painter::Text` / `Painter::TextBox` ([Painter](Painter.md#text)). Set `GameSettings::font` to
a path relative to the asset root and include it in `settings.assets` for startup validation. An empty path means no
game font: shapes still work, but font-dependent drawing requires a font. The starter game and worker ship a Noto Sans
font and its OFL license automatically.

```cpp
settings.font = "Fonts/MyFont.ttf";
settings.assets = {settings.font};
```

Glyphs are rasterized at their on-screen size when first needed. The full font character set is available; there is no
authored codepoint list or preloaded size list. Missing characters use the font's fallback glyph.

## Drawing and layout

```cpp
paint.Text("Ready", {640.0f, 80.0f}, CenteredText(32.0f, Rgb(0xffffff), 600.0f));
paint.TextBox(description, bounds,
              {.size = 20.0f, .color = Rgb(0xdddddd), .wrap = TextWrap::Word, .lineHeight = 24.0f});
```

Positions are the top of the first line. `TextAlign` selects left, center or right anchoring. `TextWrap::Word` wraps
within `maxWidth`; `TextFit::Shrink` fits without enlarging. `TextBox` takes its width from the supplied area; use a
`paint.Clip(area)` scope when height must also be constrained. `lineHeight` is an explicit distance between lines;
otherwise `lineSpacing` scales the font's line height.

Measure with `paint.Font().Measure(label, size)` or `FontFace::FittedSize`. Measurements need no GPU and do not depend
on the window size. `FontFace::Layout` returns an owned `TextBlock` for repeated drawing with `paint.Text(font, block,
position, style)`. Rebuild a retained block when its text, font or layout changes; string-based Painter calls already
use the renderer's layout cache. Effects are `TextStyle::glow`, `outline`, `shadow` and `emission`.

## Localization

Use the standalone [Localization module](../Modules/Localization/README.md), linked as `EngineLocalization`. Its public
entry point is `Modules/Localization/Localization.h`. The module owns persisted language identities, native display
names, translation fallback, placeholders and number formatting. The consumer supplies language declarations, fallback
and constexpr translation rows, then constructs an immutable catalogue.

Call `Lookup(language, key)` or `Format(language, key, arguments)` with consumer enum keys. `AvailableLanguages()`
supplies enabled options in authored order; `NormalizeLanguage` and `StepLanguage` handle selection without storing the
active language. Game content, gameplay mappings and persistence remain outside the module.

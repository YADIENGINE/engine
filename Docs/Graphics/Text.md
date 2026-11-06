# Text and fonts

Text uses `FontFace` and `Painter::Text` / `Painter::TextBox` ([Painter](Painter.md#text)). Set `font` in
`Game/Game.settings.json` ([Project settings](../ProjectSettings.md)) to a path relative to the asset root; the engine
ships it and checks it at startup. An empty path means no game font: shapes still work, but font-dependent drawing
requires a font. The starter game and worker ship a Noto Sans
font and its OFL license automatically.

```json
{
  "font": "Fonts/MyFont.ttf"
}
```

Glyphs are rasterized at their on-screen size when first needed. The full font character set is available; there is no
authored codepoint list or preloaded size list. Characters neither the font nor its [fallbacks](#other-scripts) have
draw as `?`.

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

## Shaping

Text is shaped before it is laid out (kb_text_shape, private to the engine): kerning and ligatures come from the font,
Arabic letters join, marks sit on their letters, right-to-left words read right to left, and lines break where Unicode
allows: at spaces, after hyphens, and between Chinese or Japanese characters, which need no spaces. A word longer than
the line breaks between characters. `letterSpacing` other than 0 turns optional ligatures off, so spaced letters stay
apart. A `TextBlock` lists each line's glyphs left to right as drawn; `Glyph::y` lifts or lowers a mark, and
`Glyph::font` names the fallback it came from.

## Other scripts

The starter font is Latin, Greek and Cyrillic. For another script, add a font that has it as a fallback: each character
takes the first font that has it, at the same em size, and lines keep the main font's height.

```cpp
settings.fontFallbacks = {"Fonts/NotoSansArabic-Regular.ttf"}; // With "font" set in Game/Game.settings.json
// Or on a font of your own, before it lays out text:
font.AddFallback(FontFace::FromFile(path));
```

Right-to-left text is not aligned to the right by itself: an Arabic or Hebrew screen sets `TextAlign::Right`. Limits:

- Directions follow a simplified bidirectional algorithm: no explicit direction marks (U+202A..U+202E, U+2066..U+2069),
  and a number right after right-to-left words inside left-to-right text stays on their right.
- Thai, Lao, Khmer and Myanmar have no word dictionary: they wrap between characters, never inside one.
- Horizontal text only. Load only fonts you ship: the shaper is not hardened against malicious font files.

## Localization

Use the standalone [Localization module](../../Core/Modules/Localization/README.md), linked as `EngineLocalization`. Its public
entry point is `Modules/Localization/Localization.h`. The module owns persisted language identities, native display
names, translation fallback, placeholders and number formatting. The consumer supplies language declarations, fallback
and constexpr translation rows, then constructs an immutable catalogue.

Call `Lookup(language, key)` or `Format(language, key, arguments)` with consumer enum keys. `AvailableLanguages()`
supplies enabled options in authored order; `NormalizeLanguage` and `StepLanguage` handle selection without storing the
active language. Game content, gameplay mappings and persistence remain outside the module.

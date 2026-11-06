# Noto Sans for other scripts

Unmodified hinted TTF files from https://github.com/notofonts/noto-fonts/tree/main/hinted/ttf:

| File | Version |
| --- | --- |
| `NotoSansArabic/NotoSansArabic-Regular.ttf` | 2.009 |
| `NotoSansHebrew/NotoSansHebrew-Regular.ttf` | 3.000 |
| `NotoSansThai/NotoSansThai-Regular.ttf` | 2.000 |

License: SIL Open Font License 1.1, `OFL.txt` (the same file as `../NotoSans/OFL.txt`).

Only the engine's text-shaping tests use them: joined Arabic letters, right-to-left order, marks placed on their letters,
and fallback fonts. They do not ship in games; a game that needs one of these scripts brings its own font and adds it
with `FontFace::AddFallback` or `GameSettings::fontFallbacks`.

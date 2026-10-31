# Release package and third-party notices

`python Engine release` builds Release and creates the package for players. `Engine/Tools/commands/release.py` does the
work; settings are in the `Project.json` `release` section. Every project produces the same layout:

```text
Release/                     (not tracked by git; in the template's .gitignore)
  MyGame/
    MyGame.exe
    Assets/                  selected by the package_assets hook (Assets/ as-is if no hook)
    ThirdPartyNotices.txt    GENERATED: Part 1 Engine + Part 2 game
    <release.files>          additional game texts (e.g. Credits.txt)
  MyGame.zip                 same folder, file delivered to the store/player
```

## Project.json

```json
"release": {
  "directory": "Release",
  "name": "MyGame",
  "notices": "Game/Distribution/GameNotices.txt",
  "files": ["Game/Distribution/AudioCredits.txt"]
}
```

| Field | Default | Meaning |
| --- | --- | --- |
| `directory` | `Release` | folder where the package and ZIP are written |
| `name` | project `name` | name of the package folder and ZIP |
| `notices` | none | game section of the notice (Part 2); if absent, the file contains only Part 1 |
| `files` | `[]` | additional files copied as-is to the package root |

A file named `ThirdPartyNotices.txt` cannot be listed in `files`; that name belongs to the generated notice.

## The notice has two parts

**Part 1: Engine** is added automatically; the game writes nothing. The engine's own license comes first
(`Engine/LICENSE`, MIT; name and version from the manifest). Components come from `Engine/ThirdParty/`: `Notices.json`
lists components (name, version, text file, source version pin, license file), and `Notices/*.txt` holds license texts;
`ThirdParty/notices.py` generates the output. Libraries included in every game built with the engine are here (raylib
and its bundled libraries, JSON, encryption). Dependencies used only in the editor or tests (ImGui, Catch2) are not
listed because they do not ship in the game.

**Part 2: game** is the project's `release.notices` file, added as-is. Every third-party item brought in by the game is
mentioned here or in a file referenced from here.

## Game side: `Game/Distribution/`

`Game/Distribution/` holds notice texts delivered to players. It is a sibling of `Game/Assets/`, but is not included in
the package automatically; only through `release.notices` and `release.files`. `python Engine new` creates
`Game/Distribution/GameNotices.txt` with a "no third-party content yet" line; remove it when adding the first external
content.

| File | Contents |
| --- | --- |
| `GameNotices.txt` (`release.notices`) | game's code libraries, fonts, images, shaders; references to separate files for long lists |
| `AudioCredits.txt`, `ArtCredits.txt` ... (`release.files`) | multi-entry content inventory: file → work, author, source, license |
| `README.md` | developer note: open licensing/source questions, verification records (not packaged) |

If an asset's license must be distributed beside it (e.g. font license `Assets/Fonts/OFL.txt`), the file stays beside
the asset, `package_assets` must select it too, and `GameNotices.txt` points to it.

### When to update

Every change adding, modifying, or removing external material in the game updates its notice in the same commit. This
rule applies equally to humans and AI: a change adding an asset or library is not complete without its notice.

| Change | Action |
| --- | --- |
| Downloaded font, image, sound, music, shader, model | entry in `GameNotices.txt` or the relevant credits file; package the license file if required |
| Game's own `FetchContent`/vendored library | component heading + full license text in `GameNotices.txt` |
| Editing existing external content (cutting, re-encoding) | add "edited for use in the game" to its entry |
| Removing external content | remove its entry |
| Content generated in the project (your own art, `AudioGeneration` sounds) | no notice required; if a credits file exists, it can be kept separately as "generated in the project" |
| Something supplied by the engine | nothing; Part 1 belongs to the engine |

Content with an unclear license or source is recorded as an open question in `Game/Distribution/README.md` before it
enters the package. The tool checks that texts exist; it does not check game content's license compliance.

### Entry format

Plain text, at most ~100 characters per line, each entry with a heading + underline:

```text
Inter font
----------
Source: https://rsms.me/inter/
License: SIL Open Font License 1.1, see Assets/Fonts/OFL.txt
Files:
  Assets/Fonts/Inter-Medium.ttf
```

For components offering multiple license options, state the chosen option in the heading (`stb_image.h (MIT option)`).
If the license requires attribution (MIT, BSD, zlib, CC BY), include the full text and copyright lines, not just a link.
File paths in credits entries are relative to the packaged `Assets/`.

## Checks

If packaging finds a problem, it stops before deleting the existing `Release/<name>/` folder and lists all problems:

- A `release.notices` or `release.files` file, or an asset selected by `package_assets`, is missing from the built
  `Assets/` (a new asset was added but Release was not built).
- An engine component's version in `Notices.json` is absent from its source pin (the dependency was upgraded without
  updating the notice), or the text file's first line does not mention that version.
- The recorded text does not contain the component's main license file in `ThirdParty/` verbatim.

The engine's own checks need no compiler, from the engine root: `python -m unittest discover -s ThirdParty/Tests` (reads
real pins and licenses) and `python -m unittest discover -s Tools/Tests -t .` (including packaging).

## Where it lives in the engine

Packaging is in `Tools/commands/release.py`; if the system grows (signing, version stamps, store depots, other platform
packages), this module becomes the `Tools/commands/release/` subpackage. Dependencies and their notices are in
`ThirdParty/` ([ThirdParty/README.md](../ThirdParty/README.md)).

```text
Engine/Tools/commands/release.py   package(): checks → package folder → ThirdPartyNotices.txt → ZIP
Engine/Tools/Tests/test_release.py
Engine/ThirdParty/
  <component>/        unmodified sources
  Notices.json      Part 1 component list: name, version, text, version pin, license file
  Notices/*.txt     per-component license texts
  notices.py        validation + Part 1 text
  Tests/            Python unittest
```

## Engine maintenance: adding or upgrading dependencies

The procedure is in `ThirdParty/README.md`. When adding or changing the version of a dependency shipped in games, update
`Notices/<component>.txt` and `Notices.json` in the same commit. Editor or test dependencies do not ship in games; do not
list them in `Notices.json`.
# ThirdParty

All external engine dependencies live here; building needs no internet access, and an engine copy is self-contained.
Sources are **unmodified** (git does not change line endings either: `.gitattributes`). Only folders not needed for
building (examples, documentation, tests) are omitted.

| Folder | Version | Source | Used by | Ships in games |
| --- | --- | --- | --- | --- |
| `raylib/` | 5.5 | https://github.com/raysan5/raylib/archive/refs/tags/5.5.tar.gz | `Engine` | Yes |
| `nlohmann_json/` | 3.12.0 | https://github.com/nlohmann/json/releases/download/v3.12.0/json.tar.xz | `EngineJsonIO` | Yes |
| `Monocypher/` | 4.0.3 | https://monocypher.org | `EngineJsonIO` (private) | Yes |
| `imgui/` | 1.92.7-docking | https://github.com/ocornut/imgui/archive/refs/tags/v1.92.7-docking.tar.gz | Editor | No |
| `rlImGui/` | 118221c | https://github.com/raylib-extras/rlImGui/archive/118221c8a532501a3c2c236b62950877cbefc2b4.tar.gz | Editor | No |
| `Catch2/` | 3.16.0 | https://github.com/catchorg/Catch2/archive/refs/tags/v3.16.0.tar.gz | Tests (`engine_use_catch2()`) | No |
| `libvterm/` | 0.3.3 | https://www.leonerd.org.uk/code/libvterm/libvterm-0.3.3.tar.gz | Tools console (`EngineTerminal`) | No |
| `NotoSans/` | 2.008 | https://github.com/notofonts/noto-fonts/tree/main/hinted/ttf/NotoSans | ToolUi fallback fonts; Painter starter game/worker font (OFL 1.1) | Starter only |

Included parts: raylib `CMakeLists.txt`, `CMakeOptions.txt`, `cmake/`, `src/`, `raylib.pc.in`, license; the full
nlohmann_json release package; imgui root `.cpp` / `.h` and license; rlImGui root sources, `extras/`, and license;
Catch2 `CMakeLists.txt`, `CMake/`, `src/`, `extras/`, and license; libvterm `include/`, `src/` (generated `.inc` tables
come ready in the release package), and `LICENSE`.

## License notice

Components shipped in games ("Yes" in the table) enter the engine section of every game's `ThirdPartyNotices.txt`:

- `Notices.json`: component list. `text` is the notice text, `pin` the file and text proving the source version
  (`{version}` is replaced by the version), `license` the license file the text must contain verbatim. Paths are
  relative to this folder. Optional `scope: "starter"` entries apply only when the distribution ships the starter
  font (`Assets/Fonts/Starter.ttf`); entries without a scope apply to every application. Other scopes are rejected.
- `Notices/<component>.txt`: first line `<name> <version> (<license>)`, then `Source:`, copyright lines, and license text;
  sublibraries bundled in the component have their own headings in the same file.
- `notices.py`: validation, scope selection and text generation; used by `Tools/commands/release.py` during packaging.
  Release selects the scope before validation and composition. Standalone notice tests validate all entries,
  including starter dependencies. Asset hooks that omit the starter font also omit its notice.

## Adding or upgrading a dependency

1. Download the source, place the table's parts in `ThirdParty/<component>/` (remove the old version completely; do not
   mix files).
2. Connect it in CMake (`add_subdirectory` or a source list); do not use `FetchContent`.
3. Update the table in this README.
4. If shipped in games, update `Notices/<component>.txt` and `Notices.json` (version, pin, license) in the same commit.
5. Run `python -m unittest discover -s ThirdParty/Tests` at the engine root.

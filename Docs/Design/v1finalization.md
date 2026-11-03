# v1 finalization

Everything left before v1, and nothing else. Finished work is not listed here: decisions stay in
[README](README.md), history stays in git. When an item is done, delete it from this file. When the v1 sections are
empty, the engine moves to 1.0.0 (D16) and this file is removed.

Last checked: 2026-11-02.

## 1. Debug (D12)

The only v1 feature not started. Nothing exists yet in the engine, the editor or the project template.
Suggested order: crash report first, since an AI can then read most crashes without opening a debugger.

- [ ] **Text crash report:** when a worker or game crashes, DbgHelp captures the stack trace (function, file, line). It
  goes to Output, to a file, and to the output of `<engine> build` and `<engine> test`.
- [ ] **VS Code settings in the template:** a new project gets `.vscode/tasks.json` (`<engine> build`) and `launch.json`
  ("Start the game with the debugger", "Attach to worker"). F5 needs no manual setup.
- [ ] **"Wait for debugger":** an editor switch. While on, a new worker starts and waits until a debugger attaches.
  Every build opens a new worker process (D9), so a breakpoint on the first frame is otherwise missed.

## 2. Publish

The code is on `main` of both repositories, but no user can reach it. The order matters.

- [ ] **Turn on immutable releases** in the engine repository's settings (off today). Without it, a published record
  can be changed later, which defeats checking downloads against it. Do this before the next step.
- [ ] **Publish the engine records:** `python Scripts/dev.py engine-releases` in the launcher repository (`--dry-run`
  previews). It opens public releases for `v0.1.0`, `v0.1.1`, `v0.2.0` and `v0.3.0`. The engine repository has no
  releases today, so no launcher can list versions online.
- [ ] **Release the engine, then the launcher:** `python Scripts/dev.py release`. The latest launcher, `v0.2.0`, predates
  on-demand engines, the trial in a project copy, in-app updates and the Cabinet look. A launcher release builds against
  the tag of the engine it ships, and `v0.3.0` lacks the `ToolUi` that the launcher now needs, so a new engine version
  comes first. If D12 lands first, this release carries it.
- [ ] **Require 2FA** in the organization's settings (off today).

Both products stay unsigned pre-releases for now. Signing and CI/CD are deferred (below), so they do not block this.

## 3. Check by hand

Tests cover these paths, or they cannot be tested without a person or a publication.

- [ ] **Engine change in the real window:** Change engine version → Test and change, on a copy of a project. Covered so
  far by `dev.py engine-test` and UI snapshots.
- [ ] **Cancel during a real trial:** the code path exists but has no test.
- [ ] **Trial on a large project:** measured only on the empty template (15 s).
- [ ] **The whole flow with published records,** after section 2: list, download, check, change.
- [ ] **In-app launcher update in the real window.**
- [ ] **First-run splash progress bar on a real setup.**
- [ ] **Close the old statuses:** D17 and D18 (and `Console.md`, `Rendering.md`) still say runtime verification is
  pending. Both systems are in daily use; confirm, then mark them done.

## Not v1

Deferred on purpose. None of these block 1.0.0.

- **Baking cache** (D10, [Visual assets](VisualAssets.md)): needs measurement first.
- **Prefabs** ([Asset library](AssetLibrary.md), D22).
- **Rest of the agent API** ([Agent API](AgentApi.md), D5): rendering without an open editor, MCP. The open-editor part
  is done through `ctl` (D20, D21), scripted input scenarios through `sim` (D26).
- **Draggable spatial tuning** (D11), **flipbook and disk cache** (D10).
- **CI/CD and SignPath signing** (D19): build and test on GitHub Actions, release from a tag.
- **llvm-mingw** (D8).
- **Automatic code migration** (`python Engine migrate`, D16): a failed trial's report is copied to an AI for now.
- **Console phase 2:** word and line selection ([Console](Console.md)).

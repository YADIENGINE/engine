---
name: build-fix
description: Fix a failed build, test run or editor refresh, or compiler warnings, using the engine tools' error summaries, test reports and editor logs.
---

# Fix a build, test or refresh

1. **Reproduce with the smallest command:** `python Engine build` (game), `python Engine refresh` (editor scenes
   while the editor is open) or `python Engine test --filter NAME` / `--label LABEL`.
2. **Read the summary, not the log:** failed builds end with `Errors (N):`, one `file:line:column: CODE message` per
   distinct error, paths relative to the project. Fix the first error of each file first; later ones often follow from
   it. Warnings fail a clean build too: fix the code, never silence a warning. The `Full log:` path below the summary
   holds everything else; read it by range, and only when the summary is not enough.
   An error inside `ThirdParty/pfr` names a struct the engine reads fields from (a pose, a `state.Fields` struct):
   `do_not_use_PFR_with_local_types` means it is declared in a function or an anonymous namespace; move it out. Other
   errors there mean it is not a plain struct (`Core/Utilities/Reflection.h`).
3. **Tests:** the run prints `Tests:` and a `Report:` path (`results.json`): open the report for failed test names and
   messages rather than scrolling output. Fix behaviour, not the assertion, unless the test is wrong.
4. **Editor-side failures:** a scene that fails to start or misbehaves after a refresh logs to the editor:
   `python Engine ctl logs --level warning`. A refresh never replaces a running scene with a failed build.
5. **Locked files:** if linking fails because an executable is in use, the editor or game is running that binary:
   scene code goes through `python Engine refresh`; for the editor's own executable ask the developer to close it.
6. **Done:** rerun the same command; report the result and anything you could not fix.

Commands, exit codes and options: `Docs/ToolsContract.md#commands`.

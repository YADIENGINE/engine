---
name: build-fix
description: Fix a failed build, test run or editor refresh, or compiler warnings, using the engine tools' error summaries, test reports and editor logs.
---

# Fix a build, test or refresh

1. **Reproduce with the smallest command:** `python Engine build` (game), `python Engine refresh` (editor scenes
   while the editor is open) or `python Engine test --filter NAME` / `--label LABEL`.
2. **Read the summary, not the log:** failed builds end with `Errors (N):`, one `file:line:column: CODE message` per
   distinct error, paths relative to the project. Fix the first error of each file first; later ones often follow from
   it. Warnings fail a clean build too: fix the code, never silence a warning.
3. **Tests:** the run prints `Tests:` and a `Report:` path (`results.json`): open the report for failed test names and
   messages rather than scrolling output. Fix behaviour, not the assertion, unless the test is wrong.
4. **Editor-side failures:** a scene that fails to start or misbehaves after a refresh logs to the editor:
   `python Engine ctl logs --level warning`. A refresh never replaces a running scene with a failed build.
5. **Locked files:** if linking fails because an executable is in use, the editor or game is running that binary:
   scene code goes through `python Engine refresh`; for the editor's own executable ask the developer to close it.
6. **Done:** rerun the same command; report the result and anything you could not fix.

Commands, exit codes and options: `Docs/ToolsContract.md#commands`.

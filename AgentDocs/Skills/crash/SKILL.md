---
name: crash
description: Diagnose a game, worker or native test crash from local reports, optionally with matching Release symbols.
---

# Diagnose a crash

1. Read the current command's `Crash report:` path. For an editor worker, use `python Engine ctl logs --level error`.
   A failed replacement leaves the previous scene running; its image does not prove that the new build succeeded.
2. Read the incident with `python Engine crash REPORT` or `python Engine crash REPORT --json`. Start with the message
   and first project-code frame. Do not search old reports or infer a location from an address alone.
3. For a partial packaged-game trace, use `python Engine crash REPORT --symbols SYMBOLS` with the exact developer
   archive. Another build with the same engine version is insufficient.
4. Fix the behavior and rerun the smallest original command. Handled errors and assertion failures use build-fix.
   Do not repeatedly run a known crashing game without a fix.
5. Report the cause, verification and any remaining missing-symbol limitation.

Locations and guarantees: `Docs/Debugging.md`.

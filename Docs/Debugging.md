# Crash reports

Game entry points, workers, simulation tools and instrumented Catch2 tests install local crash reporting. Ordinary
C++ exceptions need no custom base class or macros in game code. Fatal entry-point exceptions, terminate, abort and
unhandled native faults produce reports. Handled validation errors and Catch2 assertions remain ordinary failures.

## Read a report

Commands print a `Crash report:` path for their own process. Double-click that path in editor Output to open its text;
agents can retrieve it with `python Engine ctl logs --level error`.

```powershell
python Engine crash REPORT
python Engine crash REPORT --json
python Engine crash REPORT --symbols Release/Symbols/BUILD-ID
```

`REPORT` is an incident directory or `report.json`. Reading or resolving never modifies the captured files. A partial
report is a successful inspection; invalid input or an unavailable resolver returns infrastructure error 2. Resolution
uses matching executable/PDB GUID and age, offline. There is no upload, telemetry, download or symbol server.

Each incident contains `report.txt`, versioned `report.json`, and, when possible, `crash.dmp`. JSON retains module
bases, offsets and PDB identities across ASLR and rebuilds. Function/file/line require matching symbols. Trace origin
distinguishes the C++ throw point, native exception context and fatal-handler fallback. A C++ dump captures the reporting
point after unwinding; the original throw frames remain in JSON/text. Optimization can omit or merge frames.

## Locations and lifetime

- `run` and `sim`: a unique launch directory under `<build>/crashes/`, containing a copy of stderr and incident
  directories. stderr still streams live; under an agent it is held back and replaced by the crash summary. A launch
  that recorded no crash is removed when it ends; the ten newest crashed launches are kept. `sim` retains JSON stdout;
  crash diagnostics use stderr.
- `test`: `<tests build>/reports/<run-id>/crashes/`. Optional `crash_reports` paths in `results.json` identify artifacts
  at run and test level; the listener supplies the test name. Existing statuses, JUnit handling and exit codes stay.
- Worker: `crashes/` under the build-output working directory, ten newest crash records kept. Worker sessions are
  distinct. Failed replacement keeps the previous worker, and the editor's error names the crash message. Immutable
  snapshots retain EXE/PDB/reporter together; a lease prevents deleting a running worker's PDB while a new build runs.
- Standalone Release: `%LOCALAPPDATA%/<engine>/Crashes/<target>/`. Healthy startup prunes completed reports to the
  latest ten. After saving and game exit, a short notification names the folder. Tools, workers, tests and release
  asset queries suppress the notification.

Every session creates its incident directory at startup and removes it on a normal exit. A killed process leaves it
behind with an empty `emergency.txt`; the next session in the same folder removes such leftovers, never a running
session's. A non-empty `emergency.txt` without `report.json` is the record of a crash whose helper failed, and stays.

`build` only compiles: it neither starts a game nor scans old crashes. `refresh` builds and replaces an editor worker.

## Release symbols

Release stays optimized and emits symbols. Engine static libraries embed debug information, including in prebuilt
packages, so the final PDB includes engine and game locations. Before replacing a package, `release` requires its
matching PDB and reporter. Player ZIPs include `EngineCrashReporter.exe`, assets and notices, and exclude PDBs by default.

The separate `Release/Symbols/<build-id>/` directory retains exact EXE/PDB files, the reporter and `Symbols.json`.
Previous builds are preserved. Keep this directory when publishing; an engine version alone cannot identify symbols.

## Test targets and limits

Engine/module test helpers enable capture automatically. A game's own Catch2 target can opt in without replacing main:

```cmake
engine_enable_test_crash_reports(MyGameTests)
```

The listener preserves Catch2's SEH chain and remains installed while that chain reports fatal errors. Contributor
verification faults isolated subprocesses without graphics or player data:

```powershell
python . test --label '^crash$'
```

The Windows x64 implementation starts a helper while healthy. Native handlers only copy fixed exception/context data
and signal it; DbgHelp, JSON and dump writing run outside the failed process. Capture waits at most ten seconds. The
first faulting thread owns the report; other threads that fault meanwhile wait instead of ending the process early.
Missing helpers or unwritable directories fall back to an emergency record or stderr and do not stop startup.

Coverage begins when the entry point installs reporting. Pre-main/post-main failures, fail-fast, external kills and
severe memory corruption can prevent full reports. Ordinary game catch/rethrow paths outside instrumented engine
boundaries may retain only the last observed throw point. Breakpoints/single-step are not crashes; Windows dispatch
to attached debuggers is preserved. Arbitrary `.dmp` analysis remains a WinDbg/Visual Studio task.

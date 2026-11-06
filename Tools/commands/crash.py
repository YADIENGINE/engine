"""Inspect a local report; optional offline symbol resolution never edits the captured artifacts."""
import json
import os
from pathlib import Path
import subprocess

from ..crash_reports import REPORTER, read_report, summary
from ..errors import EXIT_OK, ToolError
from ..manifest import ENGINE_ROOT


def crash(args) -> int:
    file = Path(args.report).resolve()
    if file.is_dir():
        file = file / "report.json"
    report = read_report(file)
    if args.symbols is not None:
        symbols = Path(args.symbols).resolve()
        if not symbols.is_dir():
            raise ToolError(f"Symbol directory does not exist: {symbols}")
        # Captured player metadata must never choose a program to execute on the developer's machine.
        candidates = [symbols / REPORTER, ENGINE_ROOT / "build/tests/Diagnostics/Debug" / REPORTER]
        if package := os.environ.get("ENGINE_PREBUILT"):
            candidates.append(Path(package) / "Diagnostics" / REPORTER)
        candidates += list((ENGINE_ROOT / "build").glob(f"*/Diagnostics/*/{REPORTER}"))
        reporter = next((path for path in candidates if path.is_file()), None)
        if reporter is None:
            raise ToolError("Crash reporter is not built; build the game's diagnostics or provide its symbol archive")
        try:
            result = subprocess.run([str(reporter), "--resolve", str(file), "--symbols", str(symbols), "--json"],
                                    capture_output=True, encoding="utf-8", errors="replace", timeout=15)
        except (OSError, subprocess.TimeoutExpired) as error:
            raise ToolError(f"Cannot run crash reporter: {error}") from error
        if result.returncode:
            raise ToolError(result.stderr.strip() or "Crash symbol resolution failed")
        try:
            report = json.loads(result.stdout)
        except ValueError as error:
            raise ToolError(f"Invalid crash reporter output: {error}") from error
    print(json.dumps(report, indent=2, ensure_ascii=False) if args.json else summary(file, report))
    return EXIT_OK

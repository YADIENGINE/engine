"""`test`: builds and runs the headless CTest suite and keeps structured results, including infrastructure failures.

Presets and the build directory come from the project's Project.json "tests" section (see Docs/ProjectSetup.md).
Every run writes `<build>/reports/<run-id>/results.json`; exit code 0 passed, 1 failed tests, 2 infrastructure.
"""

import json
import re
import subprocess
import time
import uuid
import xml.etree.ElementTree as ET
from datetime import datetime, timezone
from pathlib import Path

from ..cmake import clear_stale_build, remove_build_dir
from ..errors import EXIT_FAILED, EXIT_INFRA, EXIT_OK
from ..msvc import msvc_env, tool


def _execute(command: list[str], root: Path, log: Path, env: dict[str, str]) -> int:
    print(f"> {subprocess.list2cmdline(command)}", flush=True)
    with log.open("w", encoding="utf-8") as output:
        try:
            process = subprocess.Popen(
                command, cwd=root, env=env, stdout=subprocess.PIPE, stderr=subprocess.STDOUT,
                text=True, encoding="utf-8", errors="replace",
            )
            assert process.stdout is not None
            with process.stdout:
                for line in process.stdout:
                    output.write(line)
                    output.flush()
            return process.wait()
        except OSError as error:
            output.write(f"Could not execute command: {error}\n")
            return 127


def _read_junit(path: Path, command: list[str]) -> list[dict]:
    document = ET.parse(path)
    results = []
    for case in document.iter("testcase"):
        failures = list(case.findall("failure")) + list(case.findall("error"))
        skipped = case.find("skipped")
        status = "failed" if failures else "skipped" if skipped is not None else "passed"
        diagnostics = []
        for element in failures + ([skipped] if skipped is not None else []):
            if element.get("message"):
                diagnostics.append(element.get("message"))
            if element.text:
                diagnostics.append(element.text)
        for tag in ("system-out", "system-err"):
            value = case.findtext(tag)
            if value:
                diagnostics.append(value)
        name = case.get("name", "<unnamed>")
        # Escape every character significant to CTest's regex engine.
        exact_filter = "^" + re.sub(r"([\\.^$|?*+()\[\]{}])", r"\\\1", name) + "$"
        rerun = command + ["--filter", exact_filter]
        results.append({
            "name": name, "status": status, "duration_seconds": float(case.get("time", "0")),
            "diagnostics": "\n".join(diagnostics), "rerun_argv": rerun,
            "rerun_command": " ".join(command) + " --filter '" + exact_filter.replace("'", "''") + "'",
        })
    return results


def run_tests(root: Path, settings: dict, command: list[str], msvc_cache: Path, test_filter: str | None = None,
              list_only: bool = False, clean: bool = False, label: str | None = None) -> int:
    """settings: Project.json "tests"; command: this command as typed from the project root
    (e.g. ["python", "Engine", "test"]), recorded with the selection for reruns."""
    started = time.monotonic()
    build = root / settings.get("buildDirectory", "build/tests")
    if clean:
        remove_build_dir(build, root)
    clear_stale_build(build, root)
    env = msvc_env(msvc_cache)
    run_id = datetime.now(timezone.utc).strftime("%Y%m%dT%H%M%SZ") + "-" + uuid.uuid4().hex[:8]
    report_dir = build / "reports" / run_id
    report_dir.mkdir(parents=True)
    report = {
        "schema_version": 1, "run_id": run_id,
        "command": command
        + (["--filter", test_filter] if test_filter is not None else [])
        + (["--label", label] if label is not None else [])
        + (["--list"] if list_only else []) + (["--clean"] if clean else []),
        "mode": "list" if list_only else "run", "stage": "configure", "status": "error",
        "counts": {"total": 0, "passed": 0, "failed": 0, "skipped": 0},
        "duration_seconds": 0, "stages": [], "tests": [], "diagnostics": "",
    }
    exit_code = EXIT_INFRA

    def execute(stage: str, command: list[str]) -> int:
        report["stage"] = stage
        log = report_dir / f"{stage}.log"
        stage_start = time.monotonic()
        code = _execute(command, root, log, env)
        report["stages"].append({
            "name": stage, "command": command, "exit_code": code,
            "duration_seconds": time.monotonic() - stage_start, "log": log.name,
        })
        if code:
            report["diagnostics"] = log.read_text(encoding="utf-8")
        return code

    try:
        if execute("configure", [tool("cmake"), "--preset", settings["configurePreset"]]):
            return exit_code
        if execute("build", [tool("cmake"), "--build", "--preset", settings["buildPreset"]]):
            return exit_code
        selection = [tool("ctest"), "--preset", settings["testPreset"]]
        if test_filter is not None:
            selection += ["--tests-regex", test_filter]
        if label is not None:
            selection += ["--label-regex", label]
        if execute("discover", selection + ["--show-only=json-v1"]):
            return exit_code
        discovered = json.loads((report_dir / "discover.log").read_text(encoding="utf-8"))
        names = [test["name"] for test in discovered["tests"]]
        if not names:
            report["diagnostics"] = "No tests matched the selection."
            return exit_code
        if list_only:
            report["status"] = "listed"
            report["tests"] = [{"name": name, "status": "listed"} for name in names]
            report["counts"]["total"] = len(names)
            print("\n".join(names))
            exit_code = EXIT_OK
            return exit_code
        code = execute("test", selection + ["--output-junit", str(report_dir / "junit.xml")])
        report["stage"] = "report"
        report["tests"] = _read_junit(report_dir / "junit.xml", command)
        actual_names = [test["name"] for test in report["tests"]]
        if sorted(actual_names) != sorted(names):
            raise ValueError("JUnit results do not match the discovered selection")
        for test in report["tests"]:
            report["counts"]["total"] += 1
            report["counts"][test["status"]] += 1
        has_failures = report["counts"]["failed"] or report["counts"]["skipped"]
        if code and not has_failures:
            raise ValueError(f"CTest exited {code} without reporting failed tests")
        report["status"] = "failed" if has_failures else "passed"
        report["stage"] = "complete"
        exit_code = EXIT_FAILED if has_failures else EXIT_OK
        return exit_code
    except (OSError, ValueError, KeyError, ET.ParseError) as error:
        report["status"] = "error"
        report["diagnostics"] = f"{error}\n" + report["diagnostics"]
        return exit_code
    finally:
        report["duration_seconds"] = time.monotonic() - started
        (report_dir / "results.json").write_text(json.dumps(report, indent=2, ensure_ascii=False) + "\n", encoding="utf-8")
        counts = report["counts"]
        if report["status"] == "listed":
            print(f"Tests: listed ({counts['total']} cases)")
        else:
            print(f"Tests: {report['status']} ({counts['passed']}/{counts['total']} passed)")
        for test in report["tests"]:
            if test["status"] in ("failed", "skipped"):
                print(f"\n{test['name']}: {test['status']}\n{test['diagnostics']}\nRerun: {test['rerun_command']}")
        if report["status"] == "error":
            print(f"Stage: {report['stage']}\n{report['diagnostics']}")
        print(f"Report: {report_dir / 'results.json'}")

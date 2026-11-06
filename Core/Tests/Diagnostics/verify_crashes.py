"""Isolated native probes: no GPU, editor, player data, or intentional fault in the test runner."""
import json
import os
from pathlib import Path
import shutil
import subprocess
import sys
import tempfile
import time


def main():
    probe, reporter = map(Path, sys.argv[1:3])
    with tempfile.TemporaryDirectory(prefix="EngineCrash-") as temporary:
        root = Path(temporary)
        for mode in ("normal", "handled", "exit", "throw", "cleanup", "unknown", "access", "thread", "threads", "terminate", "abort"):
            destination = root / mode
            env = os.environ | {"ENGINE_CRASH_DIRECTORY": str(destination)}
            result = subprocess.run([str(probe), mode], env=env, capture_output=True, timeout=15)
            reports = list(destination.glob("*/report.json"))
            if mode in ("normal", "handled", "exit"):
                assert result.returncode == 0 and not reports, (mode, result.stderr)
                assert not list(destination.glob("*/emergency.txt"))
                continue
            assert result.returncode != 0 and len(reports) == 1, (mode, result.returncode, result.stderr)
            report = json.loads(reports[0].read_text(encoding="utf-8"))
            assert report["context"] == "probe-context", report
            assert report["frames"], report
            assert reports[0].with_name("crash.dmp").stat().st_size > 0
            expected = "ThrowFromProbe" if mode in ("throw", "cleanup") else "ViolateMemory" if mode in ("access", "thread", "threads") else None
            if expected:
                frames = [f for f in report["frames"] if expected in f.get("function", "")]
                assert frames and frames[0].get("file", "").endswith("CrashProbe.cpp") and frames[0].get("line"), report
            if mode == "cleanup":
                assert report["message"] == "deliberate probe exception", report
            print(f"Crash probe {mode}: passed")
        # A killed process leaves an empty emergency file; the next session removes it and keeps crash records.
        shared = root / "shared"
        for name, emergency in (("20000101T000000-000-1-1", b""), ("20000101T000000-000-2-2", b"Crash: code=0x1\n")):
            (shared / name).mkdir(parents=True)
            (shared / name / "emergency.txt").write_bytes(emergency)
            (shared / name / "report.txt").write_bytes(b"")
        result = subprocess.run([str(probe), "normal"], env=os.environ | {"ENGINE_CRASH_DIRECTORY": str(shared)},
                                capture_output=True, timeout=15)
        assert result.returncode == 0, result.stderr
        assert sorted(item.name for item in shared.iterdir()) == ["20000101T000000-000-2-2"], list(shared.iterdir())
        print("Stale incident cleanup: passed")
        # The packaged executable has no PDB. Resolve later with the exact developer archive.
        package = root / "package"
        package.mkdir()
        packaged = package / probe.name
        shutil.copy2(probe, packaged)
        shutil.copy2(reporter, package / "EngineCrashReporter.exe")
        destination = root / "packaged"
        subprocess.run([str(packaged), "throw"], env=os.environ | {"ENGINE_CRASH_DIRECTORY": str(destination)},
                       capture_output=True, timeout=15)
        file = next(destination.glob("*/report.json"))
        original = file.read_bytes()
        # A copied EXE can still reference a live build's PDB by its absolute CodeView path. Test missing symbols
        # deterministically by resolving against an empty directory, then against the matching archive.
        empty = root / "empty"
        empty.mkdir()
        def resolve(symbols):
            result = subprocess.run([str(reporter), "--resolve", str(file), "--symbols", str(symbols), "--json"],
                                    capture_output=True, timeout=15, check=True)
            return json.loads(result.stdout)
        missing = resolve(empty)
        assert missing["status"] == "partial"
        resolved = resolve(probe.parent)
        assert any("ThrowFromProbe" in frame.get("function", "") and frame.get("line") for frame in resolved["frames"]), resolved
        assert file.read_bytes() == original
        wrong = root / "wrong"
        wrong.mkdir()
        shutil.copy2(probe, wrong / probe.name)
        shutil.copy2(reporter.with_suffix(".pdb"), wrong / probe.with_suffix(".pdb").name)
        mismatched = resolve(wrong)
        assert not any("ThrowFromProbe" in frame.get("function", "") for frame in mismatched["frames"])
        # Missing helper must leave an emergency report and exit promptly.
        result = subprocess.run([str(probe), "access"], env=os.environ | {
            "ENGINE_CRASH_DIRECTORY": str(root / "fallback"), "ENGINE_CRASH_REPORTER": str(root / "missing.exe")},
            capture_output=True, timeout=15)
        assert result.returncode and list((root / "fallback").glob("*/emergency.txt")), result.stderr
        blocked = root / "not-a-directory"
        blocked.write_text("blocked")
        result = subprocess.run([str(probe), "access"], env=os.environ | {"ENGINE_CRASH_DIRECTORY": str(blocked)},
                                capture_output=True, timeout=15)
        assert result.returncode and b"Crash:" in result.stderr, result.stderr
        started = time.monotonic()
        result = subprocess.run([str(probe), "access"], env=os.environ | {
            "ENGINE_CRASH_DIRECTORY": str(root / "timeout"), "ENGINE_CRASH_REPORTER": str(probe.resolve())},
            capture_output=True, timeout=15)
        assert result.returncode and 9 <= time.monotonic() - started < 15, result.stderr
        assert list((root / "timeout").glob("*/emergency.txt"))
        if len(sys.argv) > 3:
            catch = Path(sys.argv[3])
            for name, failed in (("Probe.Expected", False), ("Probe.Fatal", True)):
                destination = root / name
                result = subprocess.run([str(catch), name, "--colour-mode", "none"],
                    env=os.environ | {"ENGINE_CRASH_DIRECTORY": str(destination)}, capture_output=True, timeout=15)
                found = list(destination.glob("*/report.json"))
                assert bool(result.returncode) == failed and bool(found) == failed, (name, result.stderr)
                if failed:
                    assert json.loads(found[0].read_text())["context"] == name
        print("Offline symbols, immutable reports, and missing-helper fallback: passed")


if __name__ == "__main__":
    main()

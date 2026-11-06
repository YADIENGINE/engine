"""Explicit source/prebuilt consumer smoke: package a native fixture and resolve its optimized Release crash."""
import contextlib
import io
import json
import os
from pathlib import Path
import subprocess
import sys
import tempfile
import zipfile

sys.path.insert(0, str(Path(__file__).resolve().parents[3]))
from Tools.cli import main as cli
from Tools.commands.release import package
from Tools.project import Project


def main():
    game, worker = (Path(value).resolve() for value in sys.argv[1:3])
    with tempfile.TemporaryDirectory(prefix="EngineReleaseCrash-") as temporary:
        root = Path(temporary)
        (root / "Project.json").write_text(json.dumps({"version": 1, "name": "CrashFixture", "hooks": "ProjectHooks.py", "release": {}}))
        # No asset query: this fixture deliberately throws before constructing a Game or opening a window.
        (root / "ProjectHooks.py").write_text("def package_assets(root):\n    return []\n")
        packaged = package(Project(root), game)
        assert not packaged.with_suffix(".pdb").exists()
        assert (packaged.parent / "EngineCrashReporter.exe").is_file()
        with zipfile.ZipFile(root / "Release/CrashFixture.zip") as archive:
            assert not any(name.endswith(".pdb") or "/Symbols/" in name for name in archive.namelist())
        archive = next((root / "Release/Symbols").iterdir())
        for executable, kind, native in ((packaged, "game", False), (packaged, "game", True), (worker, "worker", False)):
            output = root / (kind + ("-native" if native else ""))
            env = os.environ | {"ENGINE_CRASH_DIRECTORY": str(output)}
            if native:
                env["CRASH_FIXTURE_NATIVE"] = "1"
            result = subprocess.run([str(executable)], cwd=executable.parent,
                env=env, capture_output=True, timeout=15)
            files = list(output.glob("*/report.json"))
            assert result.returncode and len(files) == 1, (kind, result.stderr)
            file = files[0]
            original = file.read_bytes()
            report = json.loads(original)
            assert report["kind"] == kind and (native or "Crash fixture" in report["message"]), report
            if kind == "game":
                assert not any(frame.get("file") for frame in report["frames"]), report
                with contextlib.redirect_stdout(io.StringIO()) as captured:
                    assert cli(["crash", str(file), "--symbols", str(archive), "--json"]) == 0
                resolved = json.loads(captured.getvalue())
                assert any(frame.get("file", "").endswith("Game.cpp") and frame.get("line")
                           for frame in resolved["frames"]), resolved
                if not native:
                    assert any(frame.get("file", "").endswith("CrashSession.cpp") and frame.get("line")
                               for frame in resolved["frames"]), "Prebuilt engine library symbols are missing"
                assert file.read_bytes() == original
            print(f"Optimized Release {kind} ({'native' if native else 'C++'}): local report passed")
        print("Player ZIP excludes symbols; archive resolves the exact packaged executable: passed")


if __name__ == "__main__":
    main()

"""Report reading, correlation, CLI errors and symbol-archive behavior; no compiler required."""
import contextlib
import io
import json
import os
from pathlib import Path
import subprocess
import tempfile
import unittest
from unittest.mock import patch

from Tools.cli import main
from Tools.crash_reports import output_summaries, read_report, reports, run_native, summary
from Tools.commands.release import archive_symbols
from Tools.errors import ToolError


class CrashReportsTests(unittest.TestCase):
    def setUp(self):
        self.temporary = tempfile.TemporaryDirectory()
        self.addCleanup(self.temporary.cleanup)
        self.root = Path(self.temporary.name)
        self.report = {"schema_version": 1, "kind": "worker", "message": "bad mesh", "status": "partial",
                       "modules": [], "frames": [{"address": 123, "function": "DrawScene", "file": "Game/Scene.cpp", "line": 42}]}

    def save(self, directory):
        directory.mkdir(parents=True, exist_ok=True)
        file = directory / "report.json"
        file.write_text(json.dumps(self.report), encoding="utf-8")
        return file

    def test_read_only_cli_keeps_partial_report_and_source(self):
        file = self.save(self.root / "crash")
        original = file.read_bytes()
        with contextlib.redirect_stdout(io.StringIO()) as output:
            code = main(["crash", str(file.parent), "--json"])
        self.assertEqual(code, 0)
        self.assertEqual(json.loads(output.getvalue()), self.report)
        self.assertEqual(file.read_bytes(), original)
        self.assertIn("Scene.cpp:42", summary(file, self.report))

    def test_invalid_report_is_infrastructure_failure(self):
        file = self.root / "report.json"
        file.write_text("{}")
        with self.assertRaises(ToolError):
            read_report(file)
        with contextlib.redirect_stderr(io.StringIO()):
            self.assertEqual(main(["crash", str(file)]), 2)

    def test_refresh_summary_preserves_cause_and_project_frame_after_a_long_trace(self):
        file = self.save(self.root / "incident")
        marker = f"Crash report: {file.with_suffix('.txt')}"
        lines = ["Crash: bad mesh", *[f"#{i} system frame" for i in range(80)], marker, marker]
        result = output_summaries(lines)
        self.assertEqual(len(result), 1)
        self.assertIn("bad mesh", result[0])
        self.assertIn("Scene.cpp:42", result[0])

    def test_player_metadata_cannot_select_the_resolver_executable(self):
        malicious = self.root / "untrusted"
        malicious.mkdir()
        (malicious / "EngineCrashReporter.exe").write_bytes(b"untrusted")
        self.report["executable"] = str(malicious / "Game.exe")
        file = self.save(self.root / "incident")
        symbols = self.root / "symbols"
        symbols.mkdir()
        with patch("Tools.commands.crash.ENGINE_ROOT", self.root / "engine"), \
                patch.dict(os.environ, {"ENGINE_PREBUILT": ""}), patch("Tools.commands.crash.subprocess.run") as execute, \
                contextlib.redirect_stderr(io.StringIO()):
            self.assertEqual(main(["crash", str(file), "--symbols", str(symbols)]), 2)
        execute.assert_not_called()

    def test_launch_reports_only_its_own_crash_and_preserves_stdout(self):
        self.save(self.root / "stale")
        executable = self.root / "Game.exe"
        def execute(command, **options):
            self.assertNotIn("stdout", options)
            destination = Path(options["env"]["ENGINE_CRASH_DIRECTORY"])
            self.save(destination / "new")
            return subprocess.CompletedProcess(command, 1)
        with patch("Tools.crash_reports.subprocess.run", side_effect=execute), \
                patch.dict(os.environ, {"ENGINE_AGENT": "1"}), contextlib.redirect_stderr(io.StringIO()) as output:
            self.assertEqual(run_native(executable, [], self.root), 1)
        self.assertEqual(output.getvalue().count("Crash (worker)"), 1)
        self.assertNotIn("stale", output.getvalue())

    def test_symbol_archive_keeps_prior_build_and_is_separate_from_package(self):
        exe = self.root / "Game.exe"
        pdb = self.root / "Game.pdb"
        helper = self.root / "EngineCrashReporter.exe"
        for file in (exe, pdb, helper):
            file.write_bytes(file.name.encode())
        identity = {"guid": "A", "age": 1, "name": pdb.name}
        first = archive_symbols(self.root / "Release", exe, identity, pdb, helper)
        original = (first / exe.name).read_bytes()
        exe.write_bytes(b"next-build")
        second = archive_symbols(self.root / "Release", exe, identity | {"guid": "B"}, pdb, helper)
        self.assertNotEqual(first, second)
        self.assertEqual((first / exe.name).read_bytes(), original)
        self.assertTrue((first / helper.name).is_file())


if __name__ == "__main__":
    unittest.main()

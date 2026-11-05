"""Short output under a coding agent: detection, cmake.run and `test`; no compiler."""

import contextlib
import io
import json
import os
import sys
import tempfile
import unittest
from pathlib import Path
from unittest.mock import patch

from Tools import agent_output
from Tools.agent_output import quiet, shorten, tail
from Tools.cmake import run
from Tools.commands.test import run_tests
from Tools.errors import ToolError

CLEAR = {"CLAUDECODE": "", "CODEX_CI": "", "ENGINE_AGENT": ""}
AGENT = {**CLEAR, "ENGINE_AGENT": "1"}
PERSON = {**CLEAR, "ENGINE_AGENT": "0"}
ERROR_LINE = "C:/x/Game.cpp(3,5): error C2065: x: undeclared"
# A compiler that writes noise, one MSVC error and fails
FAILING = [sys.executable, "-c", f"print('noise 1'); print('noise 2'); print(r'{ERROR_LINE}'); exit(1)"]
SUCCEEDING = [sys.executable, "-c", "print('compiling'); print('linking')"]


class DetectionTests(unittest.TestCase):
    def test_agents_and_the_override(self):
        cases = [({}, False), ({"CLAUDECODE": "1"}, True), ({"CODEX_CI": "1"}, True), ({"ENGINE_AGENT": "1"}, True),
                 ({"CLAUDECODE": "1", "ENGINE_AGENT": "0"}, False)]
        for values, expected in cases:
            with self.subTest(values=values), patch.dict(os.environ, {**CLEAR, **values}):
                self.assertEqual(quiet(), expected)

    def test_tail_and_shorten_say_what_they_left_out(self):
        lines = [str(number) for number in range(50)]
        self.assertEqual(tail(lines, 3), ["... last 3 of 50 lines:", "47", "48", "49"])
        self.assertEqual(tail(lines[:2], 3), ["0", "1"])
        self.assertEqual(shorten("a\nb\nc", 2), "a\nb\n... 1 more lines")


class RunTests(unittest.TestCase):
    def setUp(self):
        directory = tempfile.TemporaryDirectory()
        self.addCleanup(directory.cleanup)
        self.root = Path(directory.name)
        self.log = self.root / "build" / "build.log"
        agent_output._written.clear()

    def run_command(self, command, environment) -> tuple[str, bool]:
        printed = io.StringIO()
        failed = False
        with patch.dict(os.environ, environment), contextlib.redirect_stdout(printed):
            try:
                run(command, self.root, dict(os.environ), log=self.log)
            except ToolError:
                failed = True
        return printed.getvalue(), failed

    def test_agent_failure_prints_the_errors_and_the_log_path(self):
        printed, failed = self.run_command(FAILING, AGENT)
        self.assertTrue(failed)
        self.assertNotIn("noise", printed)
        self.assertIn("Errors (1):", printed)
        self.assertIn("C2065", printed)
        self.assertIn("Full log:", printed)
        self.assertIn("build.log", printed)
        self.assertIn("noise 2", self.log.read_text(encoding="utf-8"))

    def test_agent_success_prints_nothing_and_logs_append_within_a_run(self):
        printed, failed = self.run_command(SUCCEEDING, AGENT)
        self.assertEqual((printed, failed), ("", False))
        self.run_command(SUCCEEDING, AGENT)  # configure, then build: one log for the command
        self.assertEqual(self.log.read_text(encoding="utf-8").splitlines().count("linking"), 2)

    def test_unrecognized_failure_shows_the_end_of_the_output(self):
        printed, failed = self.run_command([sys.executable, "-c", "print('odd'); exit(3)"], AGENT)
        self.assertTrue(failed)
        self.assertIn("odd", printed)

    def test_a_person_sees_the_whole_output(self):
        printed, _ = self.run_command(FAILING, PERSON)
        self.assertIn("noise 1", printed)
        self.assertIn("Errors (1):", printed)
        self.assertNotIn("Full log", printed)


class TestCommandTests(unittest.TestCase):
    def run_failing_build(self, environment) -> str:
        with tempfile.TemporaryDirectory() as directory:
            root = Path(directory)

            def execute(command, cwd, log, env):
                text = "\n".join(f"noise {n}" for n in range(100)) + "\n" + ERROR_LINE + "\n"
                log.write_text(text if log.stem == "build" else "", encoding="utf-8")
                return 1 if log.stem == "build" else 0

            settings = {"configurePreset": "t", "buildPreset": "t", "testPreset": "t", "buildDirectory": "build/tests"}
            printed = io.StringIO()
            with patch.dict(os.environ, environment), patch("Tools.commands.test._execute", side_effect=execute), \
                    patch("Tools.commands.test.msvc_env", return_value={}), \
                    patch("Tools.commands.test.tool", side_effect=lambda name: name), \
                    contextlib.redirect_stdout(printed):
                run_tests(root, settings, ["python", "Engine", "test"], root / "msvc-env.json")
            report = json.loads(next((root / "build/tests/reports").glob("*/results.json")).read_text(encoding="utf-8"))
            self.assertIn("noise 99", report["diagnostics"])  # The report keeps everything
            return printed.getvalue()

    def test_agent_gets_the_errors_not_the_build_log(self):
        printed = self.run_failing_build(AGENT)
        self.assertNotIn("noise 50", printed)
        self.assertIn("Errors (1):", printed)
        self.assertIn("build.log", printed)
        self.assertIn("noise 50", self.run_failing_build(PERSON))

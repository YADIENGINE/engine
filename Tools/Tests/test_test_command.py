"""`test` reporting and exit-code contract; no compiler required."""

import contextlib
import io
import json
import tempfile
import unittest
from pathlib import Path
from unittest.mock import patch

from Tools.commands.test import run_tests

SETTINGS = {"configurePreset": "tests", "buildPreset": "tests-debug", "testPreset": "tests-debug",
            "buildDirectory": "build/tests"}
COMMAND = ["python", "Engine", "test"]
MODULE = "Tools.commands.test"


class RunnerTests(unittest.TestCase):
    def simulate(self, *, build_code=0, tests=None, junit=None, test_code=0,
                 test_filter=None, label=None, list_only=False):
        with tempfile.TemporaryDirectory(prefix="EngineRunner-") as directory:
            root = Path(directory)

            def execute(command, cwd, log, env):
                log.write_text("", encoding="utf-8")
                if log.stem == "build" and build_code:
                    log.write_text("compiler error: deliberately invalid fixture", encoding="utf-8")
                    return build_code
                if log.stem == "discover":
                    log.write_text(json.dumps({"tests": [{"name": name} for name in (tests or [])]}), encoding="utf-8")
                if log.stem == "test":
                    if junit is not None:
                        (log.parent / "junit.xml").write_text(junit, encoding="utf-8")
                    return test_code
                return 0

            with patch(f"{MODULE}._execute", side_effect=execute), patch(f"{MODULE}.msvc_env", return_value={}), \
                    patch(f"{MODULE}.tool", side_effect=lambda name: name), contextlib.redirect_stdout(io.StringIO()):
                code = run_tests(root, SETTINGS, COMMAND, root / "msvc-env.json", test_filter=test_filter, label=label,
                                 list_only=list_only)
            reports = list((root / "build/tests/reports").glob("*/results.json"))
            self.assertEqual(len(reports), 1)
            return code, json.loads(reports[0].read_text(encoding="utf-8"))

    def test_build_failure_is_infrastructure_error_with_log(self):
        code, report = self.simulate(build_code=1)
        self.assertEqual(code, 2)
        self.assertEqual(report["stage"], "build")
        self.assertEqual(report["status"], "error")
        self.assertIn("compiler error", report["diagnostics"])

    def test_empty_selection_is_error(self):
        code, report = self.simulate(tests=[])
        self.assertEqual(code, 2)
        self.assertIn("No tests matched", report["diagnostics"])

    def test_pass_has_counts_and_rerun(self):
        code, report = self.simulate(tests=["Session.Food"], junit='<testsuite><testcase name="Session.Food" time="0.25"/></testsuite>')
        self.assertEqual(code, 0)
        self.assertEqual(report["counts"]["passed"], 1)
        self.assertEqual(report["tests"][0]["rerun_argv"][-1], r"^Session\.Food$")
        self.assertEqual(report["tests"][0]["rerun_command"], r"python Engine test --filter '^Session\.Food$'")

    def test_failure_preserves_assertion_diagnostics(self):
        code, report = self.simulate(
            tests=["Session.Food"], test_code=8,
            junit='<testsuite><testcase name="Session.Food"><failure message="failed"/><system-out>SessionTests.cpp:41 expected 20 got 10 seed=1729</system-out></testcase></testsuite>',
        )
        self.assertEqual(code, 1)
        self.assertEqual(report["status"], "failed")
        self.assertIn("expected 20 got 10", report["tests"][0]["diagnostics"])

    def test_timeout_and_crash_are_failed_cases(self):
        for diagnostic in ("Timeout", "Exit code 0xc0000005"):
            with self.subTest(diagnostic=diagnostic):
                code, report = self.simulate(tests=["Fixture"], test_code=8, junit=f'<testsuite><testcase name="Fixture"><failure message="{diagnostic}"/></testcase></testsuite>')
                self.assertEqual(code, 1)
                self.assertIn(diagnostic, report["tests"][0]["diagnostics"])

    def test_skipped_is_not_success(self):
        code, report = self.simulate(tests=["Fixture"], junit='<testsuite><testcase name="Fixture"><skipped>not run</skipped></testcase></testsuite>')
        self.assertEqual(code, 1)
        self.assertEqual(report["counts"]["skipped"], 1)

    def test_missing_malformed_or_incomplete_junit_is_error(self):
        for junit in (None, "broken", "<testsuite/>"):
            with self.subTest(junit=junit):
                code, report = self.simulate(tests=["Fixture"], junit=junit)
                self.assertEqual(code, 2)
                self.assertEqual(report["stage"], "report")
                self.assertEqual(report["status"], "error")

    def test_ctest_error_without_case_failure_is_error(self):
        code, report = self.simulate(tests=["Fixture"], test_code=8, junit='<testsuite><testcase name="Fixture"/></testsuite>')
        self.assertEqual(code, 2)
        self.assertEqual(report["status"], "error")

    def test_label_selection_keeps_different_name_prefixes(self):
        names = ["Rewards.Card", "Legendary.Card", "Chaos.Card", "Rituals.Card"]
        junit = "<testsuite>" + "".join(f'<testcase name="{name}"/>' for name in names) + "</testsuite>"
        code, report = self.simulate(tests=names, junit=junit, label="^rewards$")
        self.assertEqual(code, 0)
        self.assertEqual(report["counts"]["passed"], len(names))
        self.assertEqual(report["command"], ["python", "Engine", "test", "--label", "^rewards$"])
        for stage in report["stages"]:
            if stage["name"] in ("discover", "test"):
                command = stage["command"]
                self.assertEqual(command[command.index("--label-regex") + 1], "^rewards$")

    def test_name_and_label_filters_are_both_applied(self):
        code, report = self.simulate(tests=["Legendary.Card"], label="^rewards$", test_filter="^Legendary\\.",
                                     junit='<testsuite><testcase name="Legendary.Card"/></testsuite>')
        self.assertEqual(code, 0)
        for stage in report["stages"]:
            if stage["name"] in ("discover", "test"):
                command = stage["command"]
                self.assertEqual(command[command.index("--tests-regex") + 1], "^Legendary\\.")
                self.assertEqual(command[command.index("--label-regex") + 1], "^rewards$")

    def test_label_listing_does_not_execute_cases(self):
        code, report = self.simulate(tests=["Chaos.Card"], label="^rewards$", list_only=True)
        self.assertEqual(code, 0)
        self.assertEqual(report["status"], "listed")
        self.assertEqual(report["tests"], [{"name": "Chaos.Card", "status": "listed"}])
        self.assertEqual([stage["name"] for stage in report["stages"]], ["configure", "build", "discover"])
        self.assertIn("--label-regex", report["stages"][-1]["command"])


if __name__ == "__main__":
    unittest.main()

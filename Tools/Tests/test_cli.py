"""The command-line contract (Docs/ToolsContract.md): commands, options, `info` and exit codes; no compiler."""

import contextlib
import io
import json
import tempfile
import unittest
from pathlib import Path
from unittest.mock import patch

from Tools import cli
from Tools.errors import EXIT_FAILED, EXIT_INFRA, EXIT_OK, ToolError
from Tools.manifest import ENGINE_ROOT, load_manifest

# Komutlar ve seçenekleri launcher'ın güvendiği sözleşmedir; buradan bir şey silmek toolsContract'ı artırır.
CONTRACT = {
    "build": ["--release", "--clean"],
    "run": ["--release"],
    "release": [],
    "test": ["--filter", "X", "--label", "X", "--list", "--clean"],
    "editor": ["--release", "--build-only", "--clean"],
    "refresh": [],
    "new": ["--name", "X", "--dry-run"],
    "info": [],
    "check": ["--fix"],
}


def run_cli(argv: list[str]) -> tuple[int, str, str]:
    out, err = io.StringIO(), io.StringIO()
    with contextlib.redirect_stdout(out), contextlib.redirect_stderr(err):
        try:
            code = cli.main(argv)
        except SystemExit as exit_:  # argparse
            code = exit_.code
    return code, out.getvalue(), err.getvalue()


class ContractTests(unittest.TestCase):
    def test_every_contract_command_and_option_parses(self):
        parser = cli.build_parser()
        for command, options in CONTRACT.items():
            with self.subTest(command=command):
                args = parser.parse_args(["--project", "Somewhere", command, *options])
                self.assertEqual(args.command, command)

    def test_usage_errors_are_infrastructure(self):
        self.assertEqual(run_cli(["unknown"])[0], EXIT_INFRA)
        self.assertEqual(run_cli([])[0], EXIT_INFRA)
        self.assertEqual(run_cli(["build", "--no-such-option"])[0], EXIT_INFRA)

    def test_launcher_name_is_reserved(self):
        # `<engine> launcher ...` launcher'ın kendi komutlarıdır; engine bu adla komut eklemez
        self.assertNotIn("launcher", cli.build_parser()._subparsers._group_actions[0].choices)

    def test_manifest_declares_the_contract_version(self):
        self.assertGreaterEqual(load_manifest().tools_contract, 1)


class InfoTests(unittest.TestCase):
    def test_describes_engine_and_project(self):
        with tempfile.TemporaryDirectory(prefix="EngineCli-") as directory:
            root = Path(directory)
            (root / "Project.json").write_text(json.dumps({"version": 1, "name": "MyGame"}), encoding="utf-8")
            code, out, _ = run_cli(["--project", str(root), "info"])
        self.assertEqual(code, EXIT_OK)
        info = json.loads(out)
        manifest = load_manifest()
        self.assertEqual(info["engine"]["name"], manifest.name)
        self.assertEqual(info["engine"]["version"], manifest.version)
        self.assertEqual(info["toolsContract"], manifest.tools_contract)
        self.assertEqual(info["project"]["name"], "MyGame")

    def test_folder_without_project_has_no_project(self):
        with tempfile.TemporaryDirectory(prefix="EngineCli-") as directory:
            code, out, _ = run_cli(["--project", directory, "info"])
        self.assertEqual(code, EXIT_OK)
        self.assertIsNone(json.loads(out)["project"])


class ErrorTests(unittest.TestCase):
    def test_missing_project_is_infrastructure(self):
        with tempfile.TemporaryDirectory(prefix="EngineCli-") as directory:
            code, _, err = run_cli(["--project", directory, "build"])
        self.assertEqual(code, EXIT_INFRA)
        self.assertIn("Project.json", err)

    def test_tool_error_code_is_returned(self):
        with patch("Tools.cli.dispatch", side_effect=ToolError("compile failed", EXIT_FAILED)):
            code, _, err = run_cli(["info"])
        self.assertEqual(code, EXIT_FAILED)
        self.assertIn("compile failed", err)


class RootTests(unittest.TestCase):
    def test_rerun_command_is_relative_to_the_project(self):
        self.assertEqual(cli.entry_command(ENGINE_ROOT.parent), ["python", ENGINE_ROOT.name])
        self.assertEqual(cli.entry_command(ENGINE_ROOT), ["python", "."])


if __name__ == "__main__":
    unittest.main()

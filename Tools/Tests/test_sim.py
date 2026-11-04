"""`sim` without a compiler: arguments and what a project without a simulation hears."""

import tempfile
import unittest
from pathlib import Path

from Tools import cli
from Tools.commands.sim import has_sources, sim, tool_arguments
from Tools.errors import EXIT_INFRA, ToolError
from Tools.project import Project


def parsed(*argv: str):
    return cli.build_parser().parse_args(["sim", *argv])


class ArgumentTests(unittest.TestCase):
    def test_tool_command_lines(self):
        self.assertEqual(tool_arguments(parsed("list")), ["list"])
        self.assertEqual(tool_arguments(parsed("run", "a.json")), ["run", "a.json"])
        self.assertEqual(tool_arguments(parsed("run", "a.json", "--out", "b.replay")),
                         ["run", "a.json", "--out", "b.replay"])
        self.assertEqual(tool_arguments(parsed("run", "a.json", "--events")), ["run", "a.json", "--events"])
        self.assertEqual(tool_arguments(parsed("verify", "b.replay")), ["verify", "b.replay"])
        self.assertEqual(tool_arguments(parsed("diverge", "b.replay")), ["diverge", "b.replay"])
        self.assertEqual(tool_arguments(parsed("state", "b.replay")), ["state", "b.replay"])
        self.assertEqual(tool_arguments(parsed("state", "b.replay", "--step", "5")),
                         ["state", "b.replay", "--step", "5"])

    def test_every_command_takes_release(self):
        for argv in (["list"], ["run", "a"], ["verify", "a"], ["diverge", "a"], ["state", "a"]):
            with self.subTest(argv=argv):
                self.assertTrue(parsed(*argv, "--release").release)
                self.assertFalse(parsed(*argv).release)


class ProjectTests(unittest.TestCase):
    def setUp(self):
        self._directory = tempfile.TemporaryDirectory(prefix="EngineSim-")
        self.root = Path(self._directory.name)

    def tearDown(self):
        self._directory.cleanup()

    def project(self, section: str = "") -> Project:
        (self.root / "Project.json").write_text('{"version": 1, "name": "MyGame"' + section + "}", encoding="utf-8")
        return Project(self.root)

    def test_a_project_without_the_section_is_told_so(self):
        with self.assertRaises(ToolError) as raised:
            sim(self.project(), parsed("list"))
        self.assertEqual(raised.exception.exit_code, EXIT_INFRA)
        self.assertIn('"simulation"', str(raised.exception))

    def test_a_project_without_sources_is_told_so(self):
        project = self.project(', "simulation": {"configurePreset": "simulation", "sources": "Game/Simulation"}')
        with self.assertRaises(ToolError) as raised:
            sim(project, parsed("list"))
        self.assertEqual(raised.exception.exit_code, EXIT_INFRA)
        self.assertIn("Game/Simulation", str(raised.exception))

    def test_sources_are_found_in_subfolders(self):
        project = self.project()
        self.assertFalse(has_sources(project, {"sources": "Game/Simulation"}))
        folder = self.root / "Game" / "Simulation" / "Rules"
        folder.mkdir(parents=True)
        (folder / "Rules.h").write_text("", encoding="utf-8")
        self.assertFalse(has_sources(project, {"sources": "Game/Simulation"}))
        (folder / "Rules.cpp").write_text("", encoding="utf-8")
        self.assertTrue(has_sources(project, {"sources": "Game/Simulation"}))

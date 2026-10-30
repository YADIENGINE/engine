"""`new` on a temporary folder; no compiler required."""

import json
import tempfile
import unittest
from pathlib import Path

from Tools.commands.new import PLACEHOLDER, default_name, name_error, setup_project, template_files
from Tools.errors import ToolError


class ProjectSetupTests(unittest.TestCase):
    def setUp(self):
        self._directory = tempfile.TemporaryDirectory(prefix="EngineNew-")
        self.root = Path(self._directory.name) / "Space Game"
        self.engine = self.root / "Engine"
        self.engine.mkdir(parents=True)

    def tearDown(self):
        self._directory.cleanup()

    def setup(self, name: str = "SpaceGame", **options):
        return setup_project(self.engine, self.root, name, **options)

    def test_creates_every_template_file_with_the_name(self):
        result = self.setup()
        expected = {relative for _, relative in template_files()}
        self.assertEqual(set(result.created), expected)
        for relative in expected:
            text = (self.root / relative).read_text(encoding="utf-8")
            self.assertNotIn(PLACEHOLDER, text, relative)
        self.assertEqual(json.loads((self.root / "Project.json").read_text(encoding="utf-8"))["name"], "SpaceGame")
        self.assertIn("project(SpaceGame ", (self.root / "CMakeLists.txt").read_text(encoding="utf-8"))

    def test_dotfiles_and_empty_folders(self):
        self.setup()
        self.assertTrue((self.root / ".gitignore").is_file())
        self.assertTrue((self.root / ".gitattributes").is_file())
        self.assertFalse((self.root / "gitignore").exists())
        self.assertTrue((self.root / "Game" / "Assets").is_dir())
        self.assertTrue((self.root / "Editor").is_dir())

    def test_no_wrapper_scripts_in_the_project(self):
        self.setup()
        self.assertEqual(list(self.root.glob("*.py")), [])

    def test_existing_files_are_kept(self):
        (self.root / "CMakeLists.txt").write_text("mine", encoding="utf-8")
        result = self.setup()
        self.assertIn(Path("CMakeLists.txt"), result.skipped)
        self.assertEqual((self.root / "CMakeLists.txt").read_text(encoding="utf-8"), "mine")

    def test_set_up_project_is_refused(self):
        self.setup()
        with self.assertRaises(ToolError):
            self.setup("Other")

    def test_dry_run_writes_nothing(self):
        result = self.setup(dry_run=True)
        self.assertTrue(result.created)
        self.assertEqual([path.name for path in self.root.iterdir()], ["Engine"])

    def test_engine_folder_must_be_named_engine(self):
        renamed = self.root / "MyEngine-main"
        self.engine.rename(renamed)
        with self.assertRaises(ToolError):
            setup_project(renamed, self.root, "SpaceGame")

    def test_names(self):
        for valid in ("SpaceGame", "Game_2", "a"):
            self.assertIsNone(name_error(valid), valid)
        for invalid in ("", "2Game", "Space Game", "Oyun-1", "Çay", "x" * 65):
            self.assertIsNotNone(name_error(invalid), invalid)
        with self.assertRaises(ToolError):
            self.setup("Space Game")

    def test_default_name_comes_from_the_folder(self):
        self.assertEqual(default_name(self.root), "SpaceGame")
        self.assertEqual(default_name(Path("123")), "MyGame")


if __name__ == "__main__":
    unittest.main()

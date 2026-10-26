"""EngineSetup.py checks on a temporary folder; no compiler required.

Run from the Tools folder: python -m unittest discover -s Tests
"""

import json
import sys
import tempfile
import unittest
from pathlib import Path

sys.path.insert(0, str(Path(__file__).resolve().parents[2]))  # Engine root, where EngineSetup.py lives

from EngineSetup import PLACEHOLDER, default_name, name_error, setup_project, template_files  # noqa: E402


class ProjectSetupTests(unittest.TestCase):
    def setUp(self):
        self._directory = tempfile.TemporaryDirectory(prefix="EngineSetup-")
        self.root = Path(self._directory.name) / "Space Game"
        self.engine = self.root / "Engine"
        self.engine.mkdir(parents=True)

    def tearDown(self):
        self._directory.cleanup()

    def test_creates_every_template_file_with_the_name(self):
        result = setup_project(self.engine, "SpaceGame")
        expected = {relative for _, relative in template_files()}
        self.assertEqual(set(result.created), expected)
        for relative in expected:
            text = (self.root / relative).read_text(encoding="utf-8")
            self.assertNotIn(PLACEHOLDER, text, relative)
        self.assertEqual(json.loads((self.root / "Project.json").read_text(encoding="utf-8"))["name"], "SpaceGame")
        self.assertIn("project(SpaceGame ", (self.root / "CMakeLists.txt").read_text(encoding="utf-8"))

    def test_dotfiles_and_empty_folders(self):
        setup_project(self.engine, "SpaceGame")
        self.assertTrue((self.root / ".gitignore").is_file())
        self.assertTrue((self.root / ".gitattributes").is_file())
        self.assertFalse((self.root / "gitignore").exists())
        self.assertTrue((self.root / "Game" / "Assets").is_dir())
        self.assertTrue((self.root / "Editor").is_dir())

    def test_existing_files_are_kept(self):
        (self.root / "CMakeLists.txt").write_text("mine", encoding="utf-8")
        result = setup_project(self.engine, "SpaceGame")
        self.assertIn(Path("CMakeLists.txt"), result.skipped)
        self.assertEqual((self.root / "CMakeLists.txt").read_text(encoding="utf-8"), "mine")

    def test_set_up_project_is_refused(self):
        setup_project(self.engine, "SpaceGame")
        with self.assertRaises(SystemExit):
            setup_project(self.engine, "Other")

    def test_dry_run_writes_nothing(self):
        result = setup_project(self.engine, "SpaceGame", dry_run=True)
        self.assertTrue(result.created)
        self.assertEqual([path.name for path in self.root.iterdir()], ["Engine"])

    def test_engine_folder_must_be_named_engine(self):
        renamed = self.root / "YDArcadeEngine-main"
        self.engine.rename(renamed)
        with self.assertRaises(SystemExit):
            setup_project(renamed, "SpaceGame")

    def test_names(self):
        for valid in ("SpaceGame", "Game_2", "a"):
            self.assertIsNone(name_error(valid), valid)
        for invalid in ("", "2Game", "Space Game", "Oyun-1", "Çay", "x" * 65):
            self.assertIsNotNone(name_error(invalid), invalid)
        with self.assertRaises(SystemExit):
            setup_project(self.engine, "Space Game")

    def test_default_name_comes_from_the_folder(self):
        self.assertEqual(default_name(self.root), "SpaceGame")
        self.assertEqual(default_name(Path("123")), "MyGame")


if __name__ == "__main__":
    unittest.main()

"""`release` packaging on temporary folders; no compiler required. Notice checks: ThirdParty/Tests."""

import json
import tempfile
import unittest
import zipfile
from pathlib import Path

from ThirdParty.notices import load_components, scoped_components
from Tools.commands.release import ENGINE_LICENSE, NOTICES_FILE, package
from Tools.errors import ToolError
from Tools.manifest import load_manifest
from Tools.project import Project


class PackageTests(unittest.TestCase):
    def setUp(self):
        self._directory = tempfile.TemporaryDirectory(prefix="Release-")
        self.root = Path(self._directory.name)
        self.exe = self.root / "build" / "Release" / "MyGame.exe"
        self.exe.parent.mkdir(parents=True)
        self.exe.write_bytes(b"exe")
        (self.exe.parent / "Assets" / "Fonts").mkdir(parents=True)
        (self.exe.parent / "Assets" / "Fonts" / "OFL.txt").write_text("font license", encoding="utf-8")
        (self.root / "Game" / "Distribution").mkdir(parents=True)
        (self.root / "Game" / "Distribution" / "GameNotices.txt").write_text("Font: see Assets/Fonts/OFL.txt.",
                                                                     encoding="utf-8")
        (self.root / "Game" / "Distribution" / "Credits.txt").write_text("credits", encoding="utf-8")

    def tearDown(self):
        self._directory.cleanup()

    def project(self, hooks: str | None = None, **release) -> Project:
        settings = {"notices": "Game/Distribution/GameNotices.txt", "files": ["Game/Distribution/Credits.txt"],
                    **release}
        data = {"version": 1, "name": "MyGame", "release": settings}
        if hooks is not None:
            (self.root / "ProjectHooks.py").write_text(hooks, encoding="utf-8")
            data["hooks"] = "ProjectHooks.py"
        (self.root / "Project.json").write_text(json.dumps(data), encoding="utf-8")
        return Project(self.root)

    def test_builds_folder_and_zip_with_both_notice_parts(self):
        exe = package(self.project(), self.exe)
        target = self.root / "Release" / "MyGame"
        self.assertEqual(exe, target / "MyGame.exe")
        notices = (target / NOTICES_FILE).read_text(encoding="utf-8")
        self.assertIn("PART 1: ENGINE", notices)
        engine = load_manifest()
        self.assertIn(f"{engine.name} {engine.version}", notices)
        self.assertIn(ENGINE_LICENSE.read_text(encoding="utf-8").strip(), notices)
        for component in scoped_components(load_components())["components"]:
            self.assertIn(component["version"], notices)
        self.assertNotIn("Noto Sans", notices)
        self.assertIn("PART 2: MYGAME", notices)
        self.assertIn("Font: see Assets/Fonts/OFL.txt.", notices)
        self.assertTrue((target / "Credits.txt").is_file())
        self.assertTrue((target / "Assets" / "Fonts" / "OFL.txt").is_file())
        with zipfile.ZipFile(self.root / "Release" / "MyGame.zip") as archive:
            self.assertIn(f"MyGame/{NOTICES_FILE}", archive.namelist())

    def test_without_game_notices_only_the_engine_part_is_written(self):
        package(self.project(notices=None), self.exe)
        notices = (self.root / "Release" / "MyGame" / NOTICES_FILE).read_text(encoding="utf-8")
        self.assertIn("PART 1: ENGINE", notices)
        self.assertNotIn("PART 2", notices)

    def test_starter_font_includes_its_notice(self):
        (self.exe.parent / "Assets/Fonts/Starter.ttf").write_bytes(b"font")
        package(self.project(), self.exe)
        notices = (self.root / "Release/MyGame" / NOTICES_FILE).read_text(encoding="utf-8")
        self.assertIn("Noto Sans", notices)

    def test_hook_can_select_the_starter_font_directory(self):
        (self.exe.parent / "Assets/Fonts/Starter.ttf").write_bytes(b"font")
        package(self.project("def package_assets(root):\n    return ['Fonts']\n"), self.exe)
        notices = (self.root / "Release/MyGame" / NOTICES_FILE).read_text(encoding="utf-8")
        self.assertIn("Noto Sans", notices)

    def test_hook_omitting_starter_font_omits_its_notice(self):
        (self.exe.parent / "Assets/Fonts/Starter.ttf").write_bytes(b"font")
        package(self.project("def package_assets(root):\n    return ['Fonts/OFL.txt']\n"), self.exe)
        notices = (self.root / "Release/MyGame" / NOTICES_FILE).read_text(encoding="utf-8")
        self.assertNotIn("Noto Sans", notices)

    def test_missing_game_notices_stop_before_touching_the_old_package(self):
        old = self.root / "Release" / "MyGame" / "old.txt"
        old.parent.mkdir(parents=True)
        old.write_text("old", encoding="utf-8")
        with self.assertRaises(ToolError):
            package(self.project(notices="Game/Distribution/Missing.txt"), self.exe)
        self.assertTrue(old.is_file())

    def test_missing_selected_asset_stops_before_touching_the_old_package(self):
        old = self.root / "Release" / "MyGame" / "old.txt"
        old.parent.mkdir(parents=True)
        old.write_text("old", encoding="utf-8")
        hooks = "def package_assets(root):\n    return ['Fonts', 'Audio/missing.ogg']\n"
        with self.assertRaises(ToolError):
            package(self.project(hooks), self.exe)
        self.assertTrue(old.is_file())

    def test_generated_notice_name_cannot_be_an_extra_file(self):
        (self.root / "Game" / "Distribution" / NOTICES_FILE).write_text("hand written", encoding="utf-8")
        with self.assertRaises(ToolError):
            package(self.project(files=[f"Game/Distribution/{NOTICES_FILE}"]), self.exe)


if __name__ == "__main__":
    unittest.main()

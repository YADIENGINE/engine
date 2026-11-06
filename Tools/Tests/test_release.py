"""`release` packaging on temporary folders; no compiler required. Notice checks: ThirdParty/Tests."""

import json
import tempfile
import unittest
import zipfile
from pathlib import Path
from unittest import mock

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
        self.pdb = self.exe.with_suffix(".pdb")
        self.pdb.write_bytes(b"symbols")
        self.reporter = self.exe.parent / "EngineCrashReporter.exe"
        self.reporter.write_bytes(b"reporter")
        patch = mock.patch("Tools.commands.release.symbol_inputs", return_value=(
            {"guid": "TEST", "age": 1, "name": self.pdb.name}, self.pdb, self.reporter))
        patch.start()
        self.addCleanup(patch.stop)
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

    def test_package_name_cannot_delete_retained_symbol_archives(self):
        retained = self.root / "Release/Symbols/previous/Game.pdb"
        retained.parent.mkdir(parents=True)
        retained.write_bytes(b"previous-symbols")
        with self.assertRaises(ToolError) as raised:
            package(self.project(name="Symbols"), self.exe)
        self.assertEqual(raised.exception.exit_code, 1)
        self.assertEqual(retained.read_bytes(), b"previous-symbols")

    def sounds(self, *names: str) -> None:
        for name in names:
            path = self.exe.parent / "Assets" / name
            path.parent.mkdir(parents=True, exist_ok=True)
            path.write_bytes(b"sound")

    def packaged(self) -> set[str]:
        assets = self.root / "Release" / "MyGame" / "Assets"
        return {path.relative_to(assets).as_posix() for path in assets.rglob("*") if path.is_file()}

    def test_only_the_sounds_a_game_uses_are_packaged(self):
        self.sounds("Audio/Hit.wav", "Audio/Unused.wav", "Audio/Music/Late.ogg", "Audio/Music/Later.ogg",
                    "Audio/Solo.wav", "Engine/Audio/Ui/UiBack.wav", "Engine/Audio/Ui/UiConfirm.wav")
        (self.exe.parent / "Assets/Audio/notes.json").write_text("{}", encoding="utf-8")
        used = ["AUDIO/hit.wav", "Engine/Audio/Ui/UiBack.wav"]  # Case does not matter on Windows
        with mock.patch("Tools.commands.release.asset_report", return_value=used):
            package(self.project(keepAssets=["Audio/Music", "Audio/Solo.wav"]), self.exe)
        self.assertEqual(self.packaged(), {
            "Audio/Hit.wav", "Audio/Music/Late.ogg", "Audio/Music/Later.ogg", "Audio/Solo.wav", "Audio/notes.json",
            "Engine/Audio/Ui/UiBack.wav", "Fonts/OFL.txt"})

    def test_a_game_with_no_used_sound_leaves_no_empty_sound_folder(self):
        self.sounds("Engine/Audio/Ui/UiBack.wav")
        with mock.patch("Tools.commands.release.asset_report", return_value=[]):
            package(self.project(), self.exe)
        self.assertFalse((self.root / "Release/MyGame/Assets/Engine").exists())

    def test_a_game_without_sounds_is_not_asked(self):
        with mock.patch("Tools.commands.release.asset_report") as report:
            package(self.project(), self.exe)
        report.assert_not_called()

    def test_a_used_or_kept_file_that_is_missing_stops_the_release(self):
        self.sounds("Audio/Hit.wav")
        previous = self.root / "Release" / "MyGame" / "old.txt"
        previous.parent.mkdir(parents=True)
        previous.write_text("old", encoding="utf-8")
        with mock.patch("Tools.commands.release.asset_report", return_value=["Audio/Gone.wav"]):
            with self.assertRaisesRegex(ToolError, r"(?s)Audio.Gone\.wav.*the game uses it"):
                package(self.project(), self.exe)
            with self.assertRaisesRegex(ToolError, r"(?s)Audio.Nope\.wav.*keepAssets"):
                package(self.project(keepAssets=["Audio/Nope.wav"]), self.exe)
        self.assertTrue(previous.is_file())

    def test_a_game_that_cannot_answer_stops_the_release(self):
        self.sounds("Audio/Hit.wav")
        # Starting the test's fake exe for real makes Windows show a dialog that waits for a click
        with mock.patch("Tools.commands.release.subprocess.run", side_effect=OSError("not a program")):
            with self.assertRaisesRegex(ToolError, "could not ask"):
                package(self.project(), self.exe)

    def test_a_hook_selects_every_asset(self):
        self.sounds("Audio/Hit.wav", "Audio/Other.wav")
        hooks = "def package_assets(root):\n    return ['Audio/Other.wav']\n"
        with mock.patch("Tools.commands.release.asset_report") as report:
            package(self.project(hooks), self.exe)
            report.assert_not_called()
            self.assertEqual(self.packaged(), {"Audio/Other.wav"})
            with self.assertRaisesRegex(ToolError, "keepAssets has no effect"):
                package(self.project(hooks, keepAssets=["Audio/Hit.wav"]), self.exe)

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

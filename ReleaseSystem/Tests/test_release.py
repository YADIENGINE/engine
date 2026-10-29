"""release.py checks on temporary folders; no compiler required.

Run from the Tools folder: python -m unittest discover -s Tests
"""

import json
import shutil
import sys
import tempfile
import unittest
import zipfile
from pathlib import Path

sys.path.insert(0, str(Path(__file__).resolve().parents[1]))  # Tools

from project_config import Project  # noqa: E402
from release import ENGINE, NOTICES_FILE, check_components, load_components, package  # noqa: E402


def copy_notices_engine(destination: Path) -> Path:
    """Notices.json, bileşen metinleri ve pin dosyalarının kopyası: denetimi gerçek Engine'e dokunmadan bozmak için."""
    manifest = load_components()
    files = {Path("Distribution") / "Notices.json"}
    for component in manifest["components"]:
        files.add(Path("Distribution") / component["text"])
        files.add(Path(component["pin"]["file"]))
    for relative in files:
        (destination / relative).parent.mkdir(parents=True, exist_ok=True)
        shutil.copy2(ENGINE / relative, destination / relative)
    return destination


class NoticeCheckTests(unittest.TestCase):
    def setUp(self):
        self._directory = tempfile.TemporaryDirectory(prefix="Release-")
        self.temp = Path(self._directory.name)

    def tearDown(self):
        self._directory.cleanup()

    def test_engine_notices_match_pinned_dependencies(self):
        self.assertEqual(check_components(load_components()), [])

    def test_changed_pin_is_reported(self):
        engine = copy_notices_engine(self.temp / "Engine")
        manifest = load_components(engine)
        component = manifest["components"][0]
        pin = engine / component["pin"]["file"]
        pinned = component["pin"]["contains"].format(version=component["version"])
        pin.write_text(pin.read_text(encoding="utf-8").replace(pinned, "changed"), encoding="utf-8")
        errors = check_components(manifest, engine)
        self.assertEqual(len(errors), 1)
        self.assertIn(component["name"], errors[0])

    def test_downloaded_license_must_appear_in_the_notice(self):
        manifest = load_components()
        component = next(c for c in manifest["components"] if "upstreamLicense" in c)
        upstream = self.temp / "_deps" / component["upstreamLicense"]
        upstream.parent.mkdir(parents=True)
        upstream.write_text("A license text that the notice does not contain.", encoding="utf-8")
        errors = check_components(manifest, deps=self.temp / "_deps")
        self.assertEqual(len(errors), 1)
        self.assertIn(component["name"], errors[0])


class PackageTests(unittest.TestCase):
    def setUp(self):
        self._directory = tempfile.TemporaryDirectory(prefix="Release-")
        self.root = Path(self._directory.name)
        self.exe = self.root / "build" / "Release" / "MyGame.exe"
        self.exe.parent.mkdir(parents=True)
        self.exe.write_bytes(b"exe")
        (self.exe.parent / "Assets" / "Fonts").mkdir(parents=True)
        (self.exe.parent / "Assets" / "Fonts" / "OFL.txt").write_text("font license", encoding="utf-8")
        (self.root / "Distribution").mkdir()
        (self.root / "Distribution" / "GameNotices.txt").write_text("Font: see Assets/Fonts/OFL.txt.",
                                                                     encoding="utf-8")
        (self.root / "Distribution" / "Credits.txt").write_text("credits", encoding="utf-8")

    def tearDown(self):
        self._directory.cleanup()

    def project(self, hooks: str | None = None, **release) -> Project:
        settings = {"notices": "Distribution/GameNotices.txt", "files": ["Distribution/Credits.txt"], **release}
        data = {"version": 1, "name": "MyGame", "release": settings}
        if hooks is not None:
            (self.root / "ProjectHooks.py").write_text(hooks, encoding="utf-8")
            data["hooks"] = "ProjectHooks.py"
        (self.root / "Project.json").write_text(json.dumps(data), encoding="utf-8")
        return Project(self.root)

    def test_builds_folder_and_zip_with_both_notice_parts(self):
        exe = package(self.project(), self.exe, self.root / "build")
        target = self.root / "Release" / "MyGame"
        self.assertEqual(exe, target / "MyGame.exe")
        notices = (target / NOTICES_FILE).read_text(encoding="utf-8")
        self.assertIn("PART 1: ENGINE", notices)
        for component in load_components()["components"]:
            self.assertIn(component["version"], notices)
        self.assertIn("PART 2: MYGAME", notices)
        self.assertIn("Font: see Assets/Fonts/OFL.txt.", notices)
        self.assertTrue((target / "Credits.txt").is_file())
        self.assertTrue((target / "Assets" / "Fonts" / "OFL.txt").is_file())
        with zipfile.ZipFile(self.root / "Release" / "MyGame.zip") as archive:
            self.assertIn(f"MyGame/{NOTICES_FILE}", archive.namelist())

    def test_without_game_notices_only_the_engine_part_is_written(self):
        package(self.project(notices=None), self.exe, self.root / "build")
        notices = (self.root / "Release" / "MyGame" / NOTICES_FILE).read_text(encoding="utf-8")
        self.assertIn("PART 1: ENGINE", notices)
        self.assertNotIn("PART 2", notices)

    def test_missing_game_notices_stop_before_touching_the_old_package(self):
        old = self.root / "Release" / "MyGame" / "old.txt"
        old.parent.mkdir(parents=True)
        old.write_text("old", encoding="utf-8")
        with self.assertRaises(SystemExit):
            package(self.project(notices="Distribution/Missing.txt"), self.exe, self.root / "build")
        self.assertTrue(old.is_file())

    def test_missing_selected_asset_stops_before_touching_the_old_package(self):
        old = self.root / "Release" / "MyGame" / "old.txt"
        old.parent.mkdir(parents=True)
        old.write_text("old", encoding="utf-8")
        hooks = "def package_assets(root):\n    return ['Fonts', 'Audio/missing.ogg']\n"
        with self.assertRaises(SystemExit):
            package(self.project(hooks), self.exe, self.root / "build")
        self.assertTrue(old.is_file())

    def test_generated_notice_name_cannot_be_an_extra_file(self):
        (self.root / "Distribution" / NOTICES_FILE).write_text("hand written", encoding="utf-8")
        with self.assertRaises(SystemExit):
            package(self.project(files=[f"Distribution/{NOTICES_FILE}"]), self.exe, self.root / "build")


if __name__ == "__main__":
    unittest.main()

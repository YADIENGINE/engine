"""notices.py checks on temporary copies; no compiler required.

Run from the Engine root: python -m unittest discover -s ThirdParty/Tests
"""

import shutil
import sys
import tempfile
import unittest
from pathlib import Path

sys.path.insert(0, str(Path(__file__).resolve().parents[1]))  # ThirdParty

from notices import NOTICES, THIRD_PARTY, check_components, compose_notices, load_components  # noqa: E402


def copy_notices(destination: Path) -> Path:
    """Notices.json, bildirim metinleri, pin ve lisans dosyalarının kopyası: denetimi gerçek kaynaklara dokunmadan
    bozmak için."""
    manifest = load_components()
    files = {Path(NOTICES)}
    for component in manifest["components"]:
        files.add(Path(component["text"]))
        files.add(Path(component["pin"]["file"]))
        if "license" in component:
            files.add(Path(component["license"]))
    for relative in files:
        (destination / relative).parent.mkdir(parents=True, exist_ok=True)
        shutil.copy2(THIRD_PARTY / relative, destination / relative)
    return destination


class NoticeCheckTests(unittest.TestCase):
    def setUp(self):
        self._directory = tempfile.TemporaryDirectory(prefix="Notices-")
        self.copy = copy_notices(Path(self._directory.name))

    def tearDown(self):
        self._directory.cleanup()

    def test_notices_match_vendored_sources(self):
        self.assertEqual(check_components(load_components()), [])

    def test_changed_pin_is_reported(self):
        manifest = load_components(self.copy)
        component = manifest["components"][0]
        pin = self.copy / component["pin"]["file"]
        pinned = component["pin"]["contains"].format(version=component["version"])
        pin.write_text(pin.read_text(encoding="utf-8").replace(pinned, "changed"), encoding="utf-8")
        errors = check_components(manifest, self.copy)
        self.assertEqual(len(errors), 1)
        self.assertIn(component["name"], errors[0])

    def test_license_must_appear_in_the_notice(self):
        manifest = load_components(self.copy)
        component = next(c for c in manifest["components"] if "license" in c)
        (self.copy / component["license"]).write_text("A license the notice does not contain.", encoding="utf-8")
        errors = check_components(manifest, self.copy)
        self.assertEqual(len(errors), 1)
        self.assertIn(component["name"], errors[0])

    def test_engine_part_names_every_component(self):
        manifest = load_components()
        notices = compose_notices(manifest, "MyGame", None)
        self.assertIn("PART 1: ENGINE", notices)
        self.assertNotIn("PART 2", notices)
        for component in manifest["components"]:
            self.assertIn(component["version"], notices)


if __name__ == "__main__":
    unittest.main()

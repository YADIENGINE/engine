"""EngineManifest.json: the real manifest follows the version / notes rules (Docs/ToolsContract.md → Manifest)."""

import copy
import json
import unittest

from Tools.manifest import ENGINE_ROOT, MANIFEST_FILE, note_problems, parse_version

VALID = {
    "version": "1.2.0",
    "repository": "https://example.com/engine.git",
    "notes": [
        {"version": "1.2.0", "date": "2026-01-02", "changes": ["Added a thing"],
         "breaking": [{"change": "Renamed X", "migrate": "Use Y"}]},
        {"version": "1.1.3", "date": "2026-01-01", "changes": ["Fixed a thing"], "breaking": []},
    ],
}


class ManifestFileTests(unittest.TestCase):
    def test_engine_manifest_follows_the_rules(self):
        data = json.loads((ENGINE_ROOT / MANIFEST_FILE).read_text(encoding="utf-8"))
        self.assertEqual(note_problems(data), [])


class NoteRuleTests(unittest.TestCase):
    def broken(self, change) -> list[str]:
        data = copy.deepcopy(VALID)
        change(data)
        return note_problems(data)

    def test_valid_manifest_has_no_problems(self):
        self.assertEqual(note_problems(VALID), [])

    def test_version_without_its_note_is_rejected(self):
        self.assertTrue(self.broken(lambda data: data.update(version="1.3.0")))

    def test_notes_are_newest_first_without_repeats(self):
        self.assertTrue(self.broken(lambda data: data["notes"].reverse()))
        self.assertTrue(self.broken(lambda data: data["notes"][1].update(version="1.2.0")))

    def test_every_note_states_its_breaking_changes(self):
        self.assertTrue(self.broken(lambda data: data["notes"][1].pop("breaking")))
        self.assertTrue(self.broken(lambda data: data["notes"][0]["breaking"][0].pop("migrate")))

    def test_dates_changes_and_repository_are_required(self):
        self.assertTrue(self.broken(lambda data: data["notes"][0].update(date="2 Jan")))
        self.assertTrue(self.broken(lambda data: data["notes"][0].update(changes="text")))
        self.assertTrue(self.broken(lambda data: data.pop("repository")))
        self.assertTrue(self.broken(lambda data: data.update(notes=[])))

    def test_versions_are_three_numbers(self):
        self.assertEqual(parse_version("0.10.2"), (0, 10, 2))
        for text in ("1.2", "01.2.3", "1.2.3-beta", ""):
            self.assertIsNone(parse_version(text))


if __name__ == "__main__":
    unittest.main()

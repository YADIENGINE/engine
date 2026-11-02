"""EngineManifest.json + ReleaseNotes/: the real files follow the version / notes rules (Docs/ToolsContract.md → Manifest)."""

import copy
import json
import tempfile
import unittest
from pathlib import Path

from Tools.errors import ToolError
from Tools.manifest import ENGINE_ROOT, MANIFEST_FILE, NOTES_FOLDER, load_notes, note_problems, parse_version

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
        self.assertEqual(note_problems(data, load_notes()), [])


class LoadNotesTests(unittest.TestCase):
    def setUp(self):
        temp = tempfile.TemporaryDirectory()
        self.addCleanup(temp.cleanup)
        self.root = Path(temp.name)
        (self.root / NOTES_FOLDER).mkdir()

    def write(self, name: str, content: str = "{}") -> None:
        (self.root / NOTES_FOLDER / name).write_text(content, encoding="utf-8")

    def test_versions_come_from_file_names_newest_first_by_number(self):
        for name in ("0.9.0.json", "0.10.0.json", "0.9.1.json"):
            self.write(name)
        self.assertEqual([note["version"] for note in load_notes(self.root)], ["0.10.0", "0.9.1", "0.9.0"])

    def test_a_file_name_that_is_no_version_is_reported(self):
        self.write("1.0.0.json", json.dumps(VALID["notes"][0]))
        self.write("draft.json", json.dumps(VALID["notes"][0]))
        problems = note_problems({"version": "1.0.0", "repository": "x"}, load_notes(self.root))
        self.assertTrue(any("draft" in problem for problem in problems))

    def test_broken_json_stops(self):
        self.write("1.0.0.json", "{")
        with self.assertRaises(ToolError):
            load_notes(self.root)


class NoteRuleTests(unittest.TestCase):
    def broken(self, change) -> list[str]:
        data = copy.deepcopy(VALID)
        change(data)
        notes = data.pop("notes")
        return note_problems(data, notes)

    def test_valid_manifest_has_no_problems(self):
        self.assertEqual(self.broken(lambda data: None), [])

    def test_version_without_its_note_is_rejected(self):
        self.assertTrue(self.broken(lambda data: data.update(version="1.3.0")))

    def test_notes_left_in_the_manifest_are_rejected(self):
        self.assertTrue(note_problems(VALID, VALID["notes"]))

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

"""`audio` and `ctl audio`: catalog usage, file lookup and the worker round trip, with a stand-in worker."""

import json
import sys
import tempfile
import textwrap
import unittest
from pathlib import Path
from types import SimpleNamespace

from Tools import cli
from Tools.commands.audio import (catalog_uses, editor_recipe, resolve_sound, rounded, run_report, usage_answer,
                                  use_label, uses_of)
from Tools.commands.ctl import audio_request, request_of
from Tools.errors import ToolError

CATALOG = {
    "effects": [{"id": 3, "file": "Audio/Hit.wav", "name": ""}, {"id": 4, "file": "Audio/hit.wav", "name": "Again"}],
    "music": [{"id": 0, "file": "Audio/Music/Menu.ogg", "name": "Menu", "group": "menu"},
              {"id": 1, "file": "Audio/Music/Gone.ogg", "name": ""}],
}
# Answers like `<worker> --audio-report REQUEST`: echoes the files and the catalog.
FAKE_WORKER = textwrap.dedent("""
    import json, sys
    request = json.load(open(sys.argv[sys.argv.index("--audio-report") + 1], encoding="utf-8"))
    if request.get("files") == ["fail"]:
        sys.exit("WorkerHost: broken request")
    answer = {"version": 1, "catalog": %s, "files": [{"file": f, "seconds": 1.23456} for f in request["files"]]}
    json.dump(answer, open(request["out"], "w", encoding="utf-8"))
""") % json.dumps(CATALOG)


class FakeProject:
    def __init__(self, root: Path):
        self.root = root
        self.name = "MyGame"

    def section(self, name: str) -> dict:
        return {"buildDirectory": "build/editor"} if name == "editor" else {}

    def path(self, relative: str) -> Path:
        return self.root / relative


class AudioCommandTests(unittest.TestCase):
    def setUp(self):
        directory = tempfile.TemporaryDirectory()
        self.addCleanup(directory.cleanup)
        self.root = Path(directory.name).resolve()
        self.assets = self.root / "Game" / "Assets"
        for name in ("Audio/Hit.wav", "Audio/Music/Menu.ogg", "Audio/Unused.mp3", "Audio/notes.txt"):
            path = self.assets / name
            path.parent.mkdir(parents=True, exist_ok=True)
            path.write_bytes(b"x")
        self.project = FakeProject(self.root)

    def write_recipe(self) -> None:
        worker = self.root / "fake_worker.py"
        worker.write_text(FAKE_WORKER, encoding="utf-8")
        recipe = self.root / "build" / "editor" / "Editor" / "Debug" / "EngineEditor.project.json"
        recipe.parent.mkdir(parents=True)
        recipe.write_text(json.dumps({"assetsDirectory": str(self.assets), "worker": {
            "executable": sys.executable, "arguments": [str(worker)], "directory": str(self.root)}}), encoding="utf-8")

    def test_parsers(self):
        args = cli.build_parser().parse_args(["audio", "usage", "Audio", "--unused"])
        self.assertEqual((args.command, args.audio_action, args.folder, args.unused), ("audio", "usage", "Audio", True))
        args = cli.build_parser().parse_args(["ctl", "audio", "play", "Audio/Hit.wav", "--at", "1.5", "--loop"])
        self.assertEqual(request_of(args), {"command": "audio-control", "action": "play", "file": "Audio/Hit.wav",
                                            "at": 1.5, "loop": True})
        args = cli.build_parser().parse_args(["ctl", "audio"])
        self.assertEqual(request_of(args), {"command": "audio"})
        self.assertEqual(audio_request(SimpleNamespace(audio_action="loop", state="off")),
                         {"command": "audio-control", "action": "loop", "on": False})

    def test_labels_and_matching_follow_the_editor(self):
        uses = catalog_uses(CATALOG)
        self.assertEqual([use["label"] for use in uses], ["Effect #3", "Effect Again", "Music Menu", "Music #1"])
        self.assertEqual(len(uses_of(uses, "audio/HIT.wav")), 2)  # Windows paths ignore case
        self.assertEqual(len(uses_of(uses, "Audio/Music/../Hit.wav")), 2)
        self.assertEqual(use_label("music", {"id": 7, "name": ""}), "Music #7")

    def test_usage_lists_unused_files_and_missing_entries(self):
        answer = usage_answer(CATALOG, self.assets, self.assets)
        self.assertEqual([sound["asset"] for sound in answer["sounds"]],
                         ["Audio/Hit.wav", "Audio/Music/Menu.ogg", "Audio/Unused.mp3"])  # Not notes.txt
        self.assertEqual(answer["sounds"][0]["uses"], ["Effect #3", "Effect Again"])
        self.assertEqual(answer["unused"], 1)
        self.assertEqual([use["label"] for use in answer["missing"]], ["Music #1"])
        self.assertEqual(usage_answer(CATALOG, self.assets, self.assets, unused=True), {"unused": ["Audio/Unused.mp3"]})
        self.assertEqual(usage_answer(CATALOG, self.assets, self.assets / "Audio" / "Music")["sounds"],
                         [{"asset": "Audio/Music/Menu.ogg", "uses": ["Music Menu"]}])

    def test_sounds_resolve_from_the_asset_folder_or_project(self):
        self.assertEqual(resolve_sound("Audio/Hit.wav", self.project, self.assets), self.assets / "Audio/Hit.wav")
        self.assertEqual(resolve_sound("Game/Assets/Audio/Hit.wav", self.project, self.assets),
                         self.assets / "Audio/Hit.wav")
        with self.assertRaises(ToolError):
            resolve_sound("Audio/Nope.wav", self.project, self.assets)

    def test_worker_round_trip(self):
        with self.assertRaises(ToolError):  # Nothing built yet
            editor_recipe(self.project)
        self.write_recipe()
        recipe = editor_recipe(self.project)
        report = run_report(self.project, recipe, [self.assets / "Audio/Hit.wav"])
        self.assertEqual(report["catalog"], CATALOG)
        self.assertEqual(rounded(report["files"][0]["seconds"]), 1.235)
        self.assertEqual(list((self.root / "build" / "editor" / "audio-report").iterdir()), [])  # No leftovers
        with self.assertRaisesRegex(ToolError, "broken request"):
            run_report(self.project, recipe, [Path("fail")])


if __name__ == "__main__":
    unittest.main()

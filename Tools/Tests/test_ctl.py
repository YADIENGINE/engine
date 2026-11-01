"""`ctl` without an editor: arguments → control requests and finding the editor; no compiler."""

import os
import unittest
from pathlib import Path
from unittest.mock import patch

from Tools import cli
from Tools.commands.ctl import parse_value, request_of
from Tools.commands.refresh import ENVIRONMENT, editor_pipe
from Tools.errors import EXIT_INFRA, ToolError


def request(*argv: str) -> dict:
    return request_of(cli.build_parser().parse_args(["ctl", *argv]))


class RequestTests(unittest.TestCase):
    def test_commands_without_arguments(self):
        for action in ("status", "list", "pause", "play", "reset", "inspect"):
            with self.subTest(action=action):
                self.assertEqual(request(action), {"command": action})

    def test_ids_and_numbers(self):
        self.assertEqual(request("open", "drawing.circle"), {"command": "open", "id": "drawing.circle"})
        self.assertEqual(request("action", "spawn"), {"command": "action", "id": "spawn"})
        self.assertEqual(request("seek", "1.5"), {"command": "seek", "seconds": 1.5})
        self.assertEqual(request("speed", "0.5"), {"command": "speed", "speed": 0.5})
        self.assertEqual(request("step"), {"command": "step", "frames": 1})
        self.assertEqual(request("step", "10"), {"command": "step", "frames": 10})

    def test_set_values_are_json_or_text(self):
        values = request("set", "radius=24", "glow=true", "tint=[1, 0, 0]", "mode=Fast", "label=a=b")["values"]
        self.assertEqual(values, {"radius": 24, "glow": True, "tint": [1, 0, 0], "mode": "Fast", "label": "a=b"})
        self.assertEqual(parse_value("0.25"), 0.25)

    def test_set_needs_id_and_value(self):
        for item in ("radius", "=3"):
            with self.subTest(item=item), self.assertRaises(ToolError) as raised:
                request("set", item)
            self.assertEqual(raised.exception.exit_code, EXIT_INFRA)

    def test_screenshot_options(self):
        self.assertEqual(request("screenshot"), {"command": "screenshot"})
        shot = request("screenshot", "--out", "shot.png", "--scale", "0.5", "--crop", "0", "0", "800", "450")
        self.assertTrue(Path(shot["path"]).is_absolute())
        self.assertEqual(shot["scale"], 0.5)
        self.assertEqual(shot["crop"], [0, 0, 800, 450])
        self.assertEqual(request("screenshot", "--window"), {"command": "screenshot", "window": True})

    def test_frames(self):
        self.assertEqual(request("frames", "--from", "0", "--to", "2"),
                         {"command": "frames", "from": 0.0, "to": 2.0, "count": 9})
        grid = request("frames", "--from", "1", "--to", "3", "--count", "4", "--columns", "4", "--scale", "0.25")
        self.assertEqual((grid["count"], grid["columns"], grid["scale"]), (4, 4, 0.25))

    def test_logs_filters(self):
        self.assertEqual(request("logs", "--since", "7", "--level", "warning"),
                         {"command": "logs", "since": 7, "limit": 100, "level": "warning"})


class EditorPipeTests(unittest.TestCase):
    def test_the_console_editor_comes_first(self):
        with patch.dict(os.environ, {ENVIRONMENT: r"\\.\pipe\EngineEditor.Control.console"}):
            self.assertEqual(editor_pipe(None), r"\\.\pipe\EngineEditor.Control.console")


if __name__ == "__main__":
    unittest.main()

"""`ctl` without an editor: arguments → control requests and finding the editor; no compiler."""

import os
import unittest
from pathlib import Path
from unittest.mock import patch

from Tools import cli
from Tools.commands.ctl import describe_scene, find_scenes, parse_value, request_of
from Tools.commands.refresh import ENVIRONMENT, editor_pipe
from Tools.errors import EXIT_FAILED, EXIT_INFRA, ToolError


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


def choice(parameter_id: str, choices: list[str], apply: str = "immediate", **extra) -> dict:
    return {"id": parameter_id, "label": parameter_id.title(), "kind": "choice", "apply": apply,
            "value": choices[0], "choices": choices, **extra}


# A small catalog in the `list` answer's shape
CATALOG = [
    {"id": "myproject.fight", "name": "Fight", "category": "Enemies", "description": "Start an enemy encounter.",
     "parameters": [choice("enemy", ["Slime", "Fire Dragon", "Ice Dragon"], "next-action"),
                    choice("stage", ["Intro", "Finale"], "next-action")],
     "actions": [{"id": "start", "label": "Start"}]},
    {"id": "myproject.dragon-portrait", "name": "Dragon Portrait", "category": "Gallery",
     "parameters": [{"id": "zoom", "label": "Zoom", "kind": "float", "apply": "immediate", "value": 1.0,
                     "minimum": 0.5, "maximum": 4.0}], "actions": []},
    {"id": "myproject.shop", "name": "Shop", "category": "Menus",
     "parameters": [choice("tab", ["Items", "Skins"], "on-reset")], "actions": []},
]


class FindTests(unittest.TestCase):
    def test_find_and_describe_read_the_list_answer(self):
        self.assertEqual(request("find", "dragon"), {"command": "list"})
        self.assertEqual(request("describe", "myproject.shop"), {"command": "list"})

    def test_a_choice_selects_the_scene_and_the_applying_action(self):
        match = find_scenes(CATALOG, "slime", 5)["matches"][0]
        self.assertEqual(match["id"], "myproject.fight")
        self.assertEqual(match["set"], {"enemy": "Slime"})
        self.assertEqual(match["commands"], ["python Engine ctl open myproject.fight",
                                             "python Engine ctl set enemy=Slime", "python Engine ctl action start"])

    def test_the_choice_most_words_match_wins_and_is_quoted(self):
        match = find_scenes(CATALOG, "ice dragon finale", 5)["matches"][0]
        self.assertEqual(match["set"], {"enemy": "Ice Dragon", "stage": "Finale"})
        self.assertIn('"enemy=Ice Dragon"', match["commands"][1])

    def test_a_named_scene_ranks_first_and_selects_no_choice(self):
        matches = find_scenes(CATALOG, "dragon portrait", 5)["matches"]
        self.assertEqual(matches[0]["id"], "myproject.dragon-portrait")
        self.assertNotIn("set", matches[0])
        self.assertEqual(matches[0]["commands"], ["python Engine ctl open myproject.dragon-portrait"])

    def test_declared_and_primary_actions_apply_values(self):
        catalog = [{**CATALOG[0], "actions": [{"id": "replay", "label": "Replay"},
                                              {"id": "begin", "label": "Begin", "primary": True}]}]
        self.assertEqual(find_scenes(catalog, "slime", 5)["matches"][0]["action"], "begin")
        catalog[0]["parameters"] = [choice("enemy", ["Slime"], "next-action", applyAction="replay")]
        self.assertEqual(find_scenes(catalog, "slime", 5)["matches"][0]["action"], "replay")
        catalog[0]["parameters"] = [choice("enemy", ["Slime"], "next-action")]
        catalog[0]["actions"] = [{"id": "replay", "label": "Replay"}, {"id": "begin", "label": "Begin"}]
        unclear = find_scenes(catalog, "slime", 5)["matches"][0]
        self.assertNotIn("action", unclear)
        self.assertIn("note", unclear)

    def test_on_reset_values_need_a_reset(self):
        self.assertEqual(find_scenes(CATALOG, "skins", 5)["matches"][0]["commands"][-1], "python Engine ctl reset")

    def test_limit_and_no_match(self):
        answer = find_scenes(CATALOG, "dragon", 1)
        self.assertEqual((len(answer["matches"]), answer["more"]), (1, 1))
        self.assertEqual(find_scenes(CATALOG, "zebra", 5)["matches"], [])
        self.assertIn("ctl list", find_scenes(CATALOG, "zebra", 5)["hint"])

    def test_describe(self):
        self.assertEqual(describe_scene(CATALOG, "myproject.shop")["scene"]["name"], "Shop")
        with self.assertRaises(ToolError) as raised:
            describe_scene(CATALOG, "myproject.shops")
        self.assertEqual(raised.exception.exit_code, EXIT_FAILED)
        self.assertIn("myproject.shop", str(raised.exception))


class EditorPipeTests(unittest.TestCase):
    def test_the_console_editor_comes_first(self):
        with patch.dict(os.environ, {ENVIRONMENT: r"\\.\pipe\EngineEditor.Control.console"}):
            self.assertEqual(editor_pipe(None), r"\\.\pipe\EngineEditor.Control.console")


if __name__ == "__main__":
    unittest.main()

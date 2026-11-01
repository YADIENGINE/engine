"""`ctl` without an editor: arguments → control requests and finding the editor; no compiler."""

import io
import os
import unittest
from pathlib import Path
from unittest.mock import patch

from Tools import cli
from Tools.commands.ctl import brief, ctl, describe_scene, find_scenes, open_scene, parse_value, request_of, with_hint
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

    def test_note(self):
        self.assertEqual(request("note", "opening", "the", "boss"),
                         {"command": "note", "text": "opening the boss", "state": "working"})
        self.assertEqual(request("note", "--state", "idle"), {"command": "note", "text": "", "state": "idle"})
        with patch.dict(os.environ, {"ENGINE_EDITOR_SESSION": "3"}):
            self.assertEqual(request("note", "x")["session"], "3")

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
        self.assertEqual(match["commands"], ["python Engine ctl open myproject.fight --set enemy=Slime --action start"
                                             " --screenshot --scale 0.5"])

    def test_the_choice_most_words_match_wins_and_is_quoted(self):
        match = find_scenes(CATALOG, "ice dragon finale", 5)["matches"][0]
        self.assertEqual(match["set"], {"enemy": "Ice Dragon", "stage": "Finale"})
        self.assertIn('--set "enemy=Ice Dragon" stage=Finale', match["commands"][0])

    def test_a_named_scene_ranks_first_and_selects_no_choice(self):
        matches = find_scenes(CATALOG, "dragon portrait", 5)["matches"]
        self.assertEqual(matches[0]["id"], "myproject.dragon-portrait")
        self.assertNotIn("set", matches[0])
        self.assertEqual(matches[0]["commands"],
                         ["python Engine ctl open myproject.dragon-portrait --screenshot --scale 0.5"])

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


class OpenTests(unittest.TestCase):
    """`open` options become a sequence of requests whose answers merge into one."""

    def run_open(self, *argv: str) -> tuple[dict, list[dict]]:
        sent = []

        def fake_send(_pipe, request_data):
            sent.append(request_data)
            match request_data["command"]:
                case "list":
                    return {"scenes": CATALOG}
                case "set":
                    return {"parameters": request_data["values"], "scene": "s"}
                case "screenshot":
                    return {"scene": "s", "path": "shot.png"}
            return {"scene": "s", "phase": request_data["command"]}

        with patch("Tools.commands.ctl.send", fake_send):
            answer = open_scene("pipe", cli.build_parser().parse_args(["ctl", "open", *argv]))
        return answer, sent

    def test_open_alone_is_one_request(self):
        answer, sent = self.run_open("myproject.fight")
        self.assertEqual(sent, [{"command": "open", "id": "myproject.fight"}])
        self.assertEqual(answer["phase"], "open")

    def test_set_action_and_screenshot_follow_in_order(self):
        answer, sent = self.run_open("myproject.fight", "--set", "enemy=Ice Dragon", "--action", "start",
                                     "--screenshot", "--scale", "0.5")
        self.assertEqual([request_data["command"] for request_data in sent],
                         ["open", "set", "list", "action", "screenshot"])
        self.assertEqual(sent[-1]["scale"], 0.5)
        self.assertEqual((answer["parameters"], answer["path"]), ({"enemy": "Ice Dragon"}, "shot.png"))

    def test_on_reset_values_reset_the_scene(self):
        _, sent = self.run_open("myproject.shop", "--set", "tab=Skins")
        self.assertEqual([request_data["command"] for request_data in sent], ["open", "set", "list", "reset"])


class AnswerTests(unittest.TestCase):
    def test_status_says_the_editor_is_open_and_what_next(self):
        idle = with_hint({"phase": "stopped", "scene": None})
        self.assertEqual(idle["editor"], "open")
        self.assertIn("ctl find", idle["next"])
        self.assertNotIn("next", with_hint({"phase": "ready", "scene": "a"}))

    def test_brief_list(self):
        self.assertEqual(brief(CATALOG)["scenes"][2], {"id": "myproject.shop", "name": "Shop", "category": "Menus"})


class HookNoteTests(unittest.TestCase):
    """`note --hook` never fails and stays silent, so an agent hook can never block the agent."""

    def run_hook(self, environment: dict, send_effect=None) -> tuple[int, list]:
        sent = []

        def fake_send(_pipe, request_data):
            sent.append(request_data)
            if send_effect:
                raise send_effect
            return {}

        args = cli.build_parser().parse_args(["ctl", "note", "--hook", "--state", "waiting"])
        with patch.dict(os.environ, environment, clear=True), patch("Tools.commands.ctl.send", fake_send), \
                patch("Tools.commands.ctl.editor_pipe", return_value="pipe"), \
                patch("sys.stdout", new_callable=io.StringIO) as output:
            code = ctl(None, args)
        self.assertEqual(output.getvalue(), "")
        return code, sent

    def test_outside_a_console_session_nothing_is_sent(self):
        self.assertEqual(self.run_hook({}), (0, []))

    def test_a_session_note_is_sent_and_failures_are_swallowed(self):
        code, sent = self.run_hook({"ENGINE_EDITOR_SESSION": "4"})
        self.assertEqual((code, sent[0]["session"], sent[0]["state"]), (0, "4", "waiting"))
        code, _ = self.run_hook({"ENGINE_EDITOR_SESSION": "4"}, ToolError("No editor is open"))
        self.assertEqual(code, 0)

    def test_a_notify_payload_is_not_note_text(self):
        sent = []
        args = cli.build_parser().parse_args(["ctl", "note", "--hook", "--state", "idle", '{"type": "turn"}'])
        with patch.dict(os.environ, {"ENGINE_EDITOR_SESSION": "4"}, clear=True), \
                patch("Tools.commands.ctl.send", lambda _pipe, data: sent.append(data) or {}), \
                patch("Tools.commands.ctl.editor_pipe", return_value="pipe"):
            self.assertEqual(ctl(None, args), 0)
        self.assertEqual(sent[0]["text"], "")


class EditorPipeTests(unittest.TestCase):
    def test_the_console_editor_comes_first(self):
        with patch.dict(os.environ, {ENVIRONMENT: r"\\.\pipe\EngineEditor.Control.console"}):
            self.assertEqual(editor_pipe(None), r"\\.\pipe\EngineEditor.Control.console")


if __name__ == "__main__":
    unittest.main()

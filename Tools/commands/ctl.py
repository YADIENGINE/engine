"""`ctl`: drive and observe the project's open editor (Docs/Editor/Automation.md).

Each subcommand sends one request to the editor's control endpoint and prints its answer as one JSON object on stdout:
list and open scenes (game flows included), change parameters, run actions, control playback, seek, take a
screenshot, read inspection values and logs. Changes show a short notice in the editor. Answers that depend on the
scene arrive after it has applied the request and drawn a new frame.

`find` and `describe` read the `list` answer here: find ranks scenes by words in their ids, names, descriptions,
parameters and choices, and spells out the commands that open the match; describe returns one scene's entry.
"""

import argparse
import difflib
import json
import re
from pathlib import Path

from ..errors import EXIT_FAILED, EXIT_OK, ToolError
from ..project import Project
from .refresh import VERSION, connect, editor_pipe, replies


def add_parser(commands) -> None:
    command = commands.add_parser("ctl", help="drive the project's open editor: scenes, playback, screenshots")
    actions = command.add_subparsers(dest="action", required=True, metavar="ACTION")
    actions.add_parser("status", help="the open scene and playback state")
    actions.add_parser("list", help="scenes and game flows with their parameters and actions")
    action = actions.add_parser("find", help="scenes matching words (also parameter choices) and how to open them")
    action.add_argument("text", nargs="+", metavar="WORD")
    action.add_argument("--limit", type=int, default=5, help="best matches to return (default 5)")
    actions.add_parser("describe", help="one scene's parameters and actions").add_argument("id")
    actions.add_parser("open", help="open a scene by id and wait until it runs").add_argument("id")
    action = actions.add_parser("set", help="set parameters of the open scene: id=value (JSON, else text)")
    action.add_argument("values", nargs="+", metavar="ID=VALUE")
    actions.add_parser("action", help="run an action of the open scene").add_argument("id")
    actions.add_parser("pause", help="pause playback")
    actions.add_parser("play", help="resume playback")
    action = actions.add_parser("step", help="pause and advance 1/60 s per frame")
    action.add_argument("frames", nargs="?", type=int, default=1)
    actions.add_parser("speed", help="playback speed (0.1-4)").add_argument("speed", type=float)
    actions.add_parser("reset", help="restart the open scene with its parameters")
    action = actions.add_parser("seek", help="pause at a playback time in seconds (replays from 0 to go back)")
    action.add_argument("seconds", type=float)
    action = actions.add_parser("screenshot", help="write the current frame as PNG and print its path")
    action.add_argument("--window", action="store_true", help="the whole editor window (crop in window pixels)")
    action.add_argument("--out", help="PNG path (default: the build folder's editor-captures/)")
    action.add_argument("--scale", type=float, help="resize factor (0.05-4)")
    action.add_argument("--crop", type=float, nargs=4, metavar=("X", "Y", "W", "H"), help="canvas-space area")
    action = actions.add_parser("frames", help="one PNG grid of frames at evenly spaced playback times")
    action.add_argument("--from", dest="start", type=float, required=True, metavar="SECONDS")
    action.add_argument("--to", dest="end", type=float, required=True, metavar="SECONDS")
    action.add_argument("--count", type=int, default=9, help="frames (2-16, default 9)")
    action.add_argument("--columns", type=int, help="tiles per row (default: about square)")
    action.add_argument("--out", help="PNG path (default: the build folder's editor-captures/)")
    action.add_argument("--scale", type=float, help="tile scale (default: the grid fits about 1600 px across)")
    action.add_argument("--crop", type=float, nargs=4, metavar=("X", "Y", "W", "H"), help="canvas-space area")
    actions.add_parser("inspect", help="the open scene's inspection values and tables")
    action = actions.add_parser("logs", help="editor, worker and build log entries")
    action.add_argument("--since", type=int, default=0, help="only entries after this sequence number")
    action.add_argument("--limit", type=int, default=100, help="newest entries to return (max 1000)")
    action.add_argument("--level", choices=("info", "warning", "error"), default="info", help="minimum level")


def parse_value(text: str):
    """JSON when it parses (numbers, true/false, [r, g, b]), otherwise the text itself (choices)."""
    try:
        return json.loads(text)
    except ValueError:
        return text


def words(text: str) -> list[str]:
    return re.findall(r"[^\W_]+", text.casefold())


def matches(term: str, text: str) -> bool:
    """A query word matches a word of the text that starts with it."""
    return any(word.startswith(term) for word in words(text))


# Weight of a match per field: a scene named for the word beats one that offers it as a choice
NAME, ID, CHOICE, CONTROL, CATEGORY, DESCRIPTION = 6, 5, 4, 3, 2, 1


def parameter_summary(parameter: dict) -> str:
    kind = parameter.get("kind", "")
    if kind == "choice":
        choices = parameter.get("choices", [])
        shown = "|".join(choices[:8]) + (f"|...(+{len(choices) - 8})" if len(choices) > 8 else "")
        return f"{shown}; now {parameter.get('value')}"
    if kind in ("integer", "float"):
        return f"{kind} {parameter.get('minimum')}..{parameter.get('maximum')}; now {parameter.get('value')}"
    return f"{kind}; now {json.dumps(parameter.get('value'), ensure_ascii=False)}"


def applying_action(scene: dict, parameters: list[dict]) -> str | None:
    """The action that applies next-action values: the declared one, else the scene's only primary or only action."""
    if not any(parameter.get("apply") == "next-action" for parameter in parameters):
        return None
    for parameter in parameters:
        if parameter.get("applyAction"):
            return parameter["applyAction"]
    actions = scene.get("actions", [])
    primary = [action for action in actions if action.get("primary")]
    if len(primary) == 1:
        return primary[0]["id"]
    return actions[0]["id"] if len(actions) == 1 else None


def shell_word(text: str) -> str:
    return f'"{text}"' if re.search(r"[\s\"'&|;<>()$`]", text) else text


def open_commands(scene: dict, values: dict, action: str | None) -> list[str]:
    commands = [f"python Engine ctl open {scene['id']}"]
    if values:
        items = " ".join(shell_word(f"{key}={value}") for key, value in values.items())
        commands.append(f"python Engine ctl set {items}")
    parameters = [parameter for parameter in scene.get("parameters", []) if parameter["id"] in values]
    if any(parameter.get("apply") == "on-reset" for parameter in parameters):
        commands.append("python Engine ctl reset")
    if action:
        commands.append(f"python Engine ctl action {action}")
    return commands


def scene_match(scene: dict, terms: list[str]) -> dict | None:
    """How well one catalog entry matches the query words; None when no word matches."""
    score, found, matched = 0, 0, []
    for term in terms:
        best, reasons = 0, []

        def field(weight: int, text: str, reason: str) -> None:
            nonlocal best
            if text and matches(term, text):
                best = max(best, weight)
                reasons.append(reason)

        field(NAME, scene.get("name", ""), "name")
        field(ID, scene["id"], "id")
        field(CATEGORY, scene.get("category", ""), "category")
        field(DESCRIPTION, scene.get("description", ""), "description")
        for parameter in scene.get("parameters", []):
            field(CONTROL, f"{parameter['id']} {parameter.get('label', '')}", f"parameter {parameter['id']}")
            field(DESCRIPTION, parameter.get("description", ""), f"parameter {parameter['id']}")
            if any(matches(term, choice) for choice in parameter.get("choices", [])):
                best = max(best, CHOICE)
        for action in scene.get("actions", []):
            field(CONTROL, f"{action['id']} {action.get('label', '')}", f"action {action['id']}")
            field(DESCRIPTION, action.get("description", ""), f"action {action['id']}")
        if best:
            score, found = score + best, found + 1
            matched += [reason for reason in reasons if reason not in matched]
    if not found:
        return None
    # Words the scene's name or id already explains select no choice ("map" opens the map scene, not a map theme).
    # Per parameter, the choice most of the remaining words match; the first one on ties.
    title = f"{scene.get('name', '')} {scene['id']}"
    choosing = [term for term in terms if not matches(term, title)]
    values = {}
    for parameter in scene.get("parameters", []):
        counts = [(sum(matches(term, choice) for term in choosing), choice) for choice in parameter.get("choices", [])]
        count, choice = max(counts, key=lambda item: item[0], default=(0, ""))
        if count:
            values[parameter["id"]] = choice
    set_parameters = [parameter for parameter in scene.get("parameters", []) if parameter["id"] in values]
    action = applying_action(scene, set_parameters)
    result = {"id": scene["id"], "name": scene.get("name", ""), "category": scene.get("category", "")}
    result["matched"] = [f"{key}={value}" for key, value in values.items()] + [
        reason for reason in matched if not reason.startswith("parameter ") or reason[10:] not in values]
    if values:
        result["set"] = values
    if action:
        result["action"] = action
    elif any(parameter.get("apply") == "next-action" for parameter in set_parameters):
        result["note"] = "these values apply when one of the scene's actions runs"
    result["commands"] = open_commands(scene, values, action)
    result["parameters"] = {parameter["id"]: parameter_summary(parameter) for parameter in scene.get("parameters", [])}
    result["actions"] = [action["id"] for action in scene.get("actions", [])]
    return {"rank": (found, score), "match": result}


def find_scenes(scenes: list[dict], text: str, limit: int) -> dict:
    """Catalog entries ranked by matched query words, then by match weight; catalog order breaks ties."""
    terms = words(text)
    ranked = [match for scene in scenes if (match := scene_match(scene, terms))]
    ranked.sort(key=lambda match: match["rank"], reverse=True)
    answer = {"query": text, "matches": [match["match"] for match in ranked[:max(limit, 1)]]}
    if len(ranked) > len(answer["matches"]):
        answer["more"] = len(ranked) - len(answer["matches"])
    if not ranked:
        answer["hint"] = f"No scene matches; {len(scenes)} scenes: python Engine ctl list"
    return answer


def describe_scene(scenes: list[dict], scene_id: str) -> dict:
    for scene in scenes:
        if scene["id"] == scene_id:
            return {"scene": scene}
    close = difflib.get_close_matches(scene_id, [scene["id"] for scene in scenes], n=3, cutoff=0.5)
    hint = f"; did you mean: {', '.join(close)}" if close else "; search with: python Engine ctl find WORD"
    raise ToolError(f"Unknown scene: {scene_id}{hint}", EXIT_FAILED)


def request_of(args: argparse.Namespace) -> dict:
    """The control request for parsed `ctl` arguments."""
    request = {"command": args.action}
    match args.action:
        case "find" | "describe":
            request["command"] = "list"
        case "open" | "action":
            request["id"] = args.id
        case "set":
            values = {}
            for item in args.values:
                key, separator, value = item.partition("=")
                if not separator or not key:
                    raise ToolError(f"Expected ID=VALUE, got: {item}")
                values[key] = parse_value(value)
            request["values"] = values
        case "step":
            request["frames"] = args.frames
        case "speed":
            request["speed"] = args.speed
        case "seek":
            request["seconds"] = args.seconds
        case "frames":
            request.update({"from": args.start, "to": args.end, "count": args.count})
            if args.columns is not None:
                request["columns"] = args.columns
            if args.out:
                request["path"] = str(Path(args.out).resolve())
            if args.scale is not None:
                request["scale"] = args.scale
            if args.crop:
                request["crop"] = args.crop
        case "screenshot":
            if args.window:
                request["window"] = True
            if args.out:
                request["path"] = str(Path(args.out).resolve())
            if args.scale is not None:
                request["scale"] = args.scale
            if args.crop:
                request["crop"] = args.crop
        case "logs":
            request.update(since=args.since, limit=args.limit, level=args.level)
    return request


def ctl(project: Project, args: argparse.Namespace) -> int:
    request = {"version": VERSION, **request_of(args)}
    with connect(editor_pipe(project)) as stream:
        stream.write((json.dumps(request) + "\n").encode("utf-8"))
        for reply in replies(stream):
            match reply.pop("type", None):
                case "result":
                    if args.action == "find":
                        reply = find_scenes(reply.get("scenes", []), " ".join(args.text), args.limit)
                    elif args.action == "describe":
                        reply = describe_scene(reply.get("scenes", []), args.id)
                    print(json.dumps(reply, ensure_ascii=False), flush=True)
                    return EXIT_OK
                case "error":
                    raise ToolError(reply.get("text", "The editor refused the request"), EXIT_FAILED)
    raise ToolError("The editor closed the connection before answering")

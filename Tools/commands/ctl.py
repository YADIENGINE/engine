"""`ctl`: drive and observe the project's open editor (Docs/Editor/Automation.md).

Each subcommand sends one request to the editor's control endpoint and prints its answer as one JSON object on stdout:
list and open previews (game flows included), change parameters, run actions, control playback, seek, take a
screenshot, read inspection values and logs. Changes show a short notice in the editor. Answers that depend on the
preview arrive after it has applied the request and drawn a new frame.
"""

import argparse
import json
from pathlib import Path

from ..errors import EXIT_FAILED, EXIT_OK, ToolError
from ..project import Project
from .refresh import VERSION, connect, editor_pipe, replies


def add_parser(commands) -> None:
    command = commands.add_parser("ctl", help="drive the project's open editor: previews, playback, screenshots")
    actions = command.add_subparsers(dest="action", required=True, metavar="ACTION")
    actions.add_parser("status", help="the open preview and playback state")
    actions.add_parser("list", help="previews and game flows with their parameters and actions")
    actions.add_parser("open", help="open a preview by id and wait until it runs").add_argument("id")
    action = actions.add_parser("set", help="set parameters of the open preview: id=value (JSON, else text)")
    action.add_argument("values", nargs="+", metavar="ID=VALUE")
    actions.add_parser("action", help="run an action of the open preview").add_argument("id")
    actions.add_parser("pause", help="pause playback")
    actions.add_parser("play", help="resume playback")
    action = actions.add_parser("step", help="pause and advance 1/60 s per frame")
    action.add_argument("frames", nargs="?", type=int, default=1)
    actions.add_parser("speed", help="playback speed (0.1-4)").add_argument("speed", type=float)
    actions.add_parser("reset", help="restart the open preview with its parameters")
    action = actions.add_parser("seek", help="pause at a playback time in seconds (replays from 0 to go back)")
    action.add_argument("seconds", type=float)
    action = actions.add_parser("screenshot", help="write the current frame as PNG and print its path")
    action.add_argument("--out", help="PNG path (default: the build folder's editor-captures/)")
    action.add_argument("--scale", type=float, help="resize factor (0.05-4)")
    action.add_argument("--crop", type=float, nargs=4, metavar=("X", "Y", "W", "H"), help="canvas-space area")
    actions.add_parser("inspect", help="the open preview's inspection values and tables")
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


def request_of(args: argparse.Namespace) -> dict:
    """The control request for parsed `ctl` arguments."""
    request = {"command": args.action}
    match args.action:
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
        case "screenshot":
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
                    print(json.dumps(reply, ensure_ascii=False), flush=True)
                    return EXIT_OK
                case "error":
                    raise ToolError(reply.get("text", "The editor refused the request"), EXIT_FAILED)
    raise ToolError("The editor closed the connection before answering")

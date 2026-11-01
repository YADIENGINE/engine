"""`refresh`: Build & Refresh in the project's open editor, as if its button were pressed.

The running scene stays live while the worker builds and is replaced once the new build is ready. The command
streams the build's output and exits with its result; when a refresh is already running it joins that one.

It talks to the editor's control endpoint (Engine/Editor/Internals/Protocol/EditorControlProtocol.h): the editor
announces a named pipe in its build directory; a request and the replies are JSON objects, one per line.
"""

import json
import os
import time
from pathlib import Path

from ..build_errors import summary
from ..errors import EXIT_FAILED, EXIT_INFRA, EXIT_OK, ToolError
from ..project import Project
from .editor import editor_build_dir

ANNOUNCEMENT = "EngineEditor.control.json"
ENVIRONMENT = "ENGINE_EDITOR_CONTROL"  # Set by the editor for its console: that editor's pipe
VERSION = 3
CONNECT_TIMEOUT = 5.0  # Seconds to wait while another client holds the endpoint
CONNECT_RETRY = 0.1
PIPE_BUSY = 231  # ERROR_PIPE_BUSY


def endpoint(build_dir: Path) -> str:
    """The pipe the project's open editor listens on."""
    path = build_dir / ANNOUNCEMENT
    if not path.is_file():
        raise ToolError("No editor is open for this project; start it with: python Engine editor")
    try:
        announcement = json.loads(path.read_text(encoding="utf-8"))
        if announcement["version"] != VERSION:
            raise ToolError(f"{path}: editor control version {announcement['version']}; this engine speaks {VERSION}")
        return announcement["pipe"]
    except (ValueError, KeyError) as error:
        raise ToolError(f"{path}: {error}") from error


def editor_pipe(project: Project) -> str:
    """The editor this command was started from (its console), else the project's announced one."""
    return os.environ.get(ENVIRONMENT) or endpoint(editor_build_dir(project))


def connect(pipe: str):
    deadline = time.monotonic() + CONNECT_TIMEOUT
    while True:
        try:
            return open(pipe, "r+b", buffering=0)
        except FileNotFoundError as error:  # The announcement outlived its editor (crash)
            raise ToolError("No editor is open for this project; start it with: python Engine editor") from error
        except OSError as error:
            if getattr(error, "winerror", None) != PIPE_BUSY or time.monotonic() >= deadline:
                raise ToolError(f"Cannot reach the editor: {error}") from error
            time.sleep(CONNECT_RETRY)


def replies(stream):
    """Reply objects as they arrive, until the editor closes the connection."""
    pending = b""
    while chunk := stream.read(4096):
        pending += chunk
        while b"\n" in pending:
            line, pending = pending.split(b"\n", 1)
            if line.strip():
                yield json.loads(line)


def exit_code(result: str) -> int:
    return EXIT_OK if result == "succeeded" else EXIT_FAILED


def refresh(project: Project) -> int:
    output: list[str] = []
    with connect(editor_pipe(project)) as stream:
        stream.write((json.dumps({"version": VERSION, "command": "refresh"}) + "\n").encode("utf-8"))
        for reply in replies(stream):
            match reply.get("type"):
                case "log":
                    print(reply["text"], flush=True)
                    output.append(reply["text"])
                case "result":
                    code = exit_code(reply["result"])
                    if code != EXIT_OK:
                        for line in summary(output, project.root):
                            print(line, flush=True)
                    print(f"> Build & Refresh: {reply['result']}", flush=True)
                    return code
                case "error":
                    raise ToolError(reply["text"], EXIT_INFRA)
    raise ToolError("The editor closed the connection before reporting a result")

"""`refresh`: Build & Refresh in the project's open editor, as if its button were pressed.

The running scene stays live while the worker builds and is replaced once the new build is ready. The command
streams the build's output and exits with its result; when a refresh is already running it joins that one.

It talks to the editor's control endpoint (Engine/Editor/Internals/Protocol/EditorControlProtocol.h): the editor
announces a named pipe in its build directory; a request and the replies are JSON objects, one per line.
"""

import errno
import json
import os
import time
from pathlib import Path

from ..agent_output import quiet, shown, tail, write_log
from ..build_errors import summary
from ..errors import EXIT_FAILED, EXIT_INFRA, EXIT_OK, ToolError
from ..project import Project
from .editor import editor_build_dir

ANNOUNCEMENT = "EngineEditor.control.json"
ENVIRONMENT = "ENGINE_EDITOR_CONTROL"  # Set by the editor for its console: that editor's pipe
SESSION_ENVIRONMENT = "ENGINE_EDITOR_SESSION"  # Set by the editor for each Console session: who is asking
VERSION = 4
CONNECT_TIMEOUT = 5.0  # Seconds to wait while every connection of the endpoint is taken
CONNECT_RETRY = 0.1
REFRESH_LOG = "refresh.log"  # In the editor build folder: the editor's build lines of the last refresh
PIPE_BUSY = errno.EINVAL  # How open() reports ERROR_PIPE_BUSY: the C runtime maps it to this, and winerror stays empty


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
            if error.errno != PIPE_BUSY or time.monotonic() >= deadline:
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


def request_line(request: dict) -> bytes:
    """One request as the editor reads it: the protocol version and, from a Console session, which session asks (the
    editor lets one session at a time change or capture it; Docs/Editor/Automation.md#several-agents)."""
    header = {"version": VERSION}
    if session := os.environ.get(SESSION_ENVIRONMENT):
        header["session"] = session
    return (json.dumps({**header, **request}) + "\n").encode("utf-8")


def refresh(project: Project) -> int:
    """Asks the open editor to build and refresh. Its build lines stream here, or under an agent go only to
    `<editor build>/refresh.log` (agent_output.py)."""
    agent = quiet()
    log = editor_build_dir(project) / REFRESH_LOG
    output: list[str] = []
    with connect(editor_pipe(project)) as stream:
        stream.write(request_line({"command": "refresh"}))
        for reply in replies(stream):
            match reply.get("type"):
                case "log":
                    if not agent:
                        print(reply["text"], flush=True)
                    output.append(reply["text"])
                case "result":
                    write_log(log, output)
                    code = exit_code(reply["result"])
                    if code != EXIT_OK:
                        for line in summary(output, project.root) or (tail(output) if agent else []):
                            print(line, flush=True)
                    where = f" (log: {shown(log)})" if agent else ""
                    print(f"> Build & Refresh: {reply['result']}{where}", flush=True)
                    return code
                case "error":
                    raise ToolError(reply["text"], EXIT_INFRA)
    raise ToolError("The editor closed the connection before reporting a result")

"""Codex session logs: `<home>/sessions/YYYY/MM/DD/rollout-*.jsonl` (and `archived_sessions/`).

Codex reports input tokens with the cached part included; Request.input holds only the uncached rest. Shell commands
run inside an `exec` script (`tools.exec_command({cmd: ...})`, `tools.shell_command({command: ...})`, or a list
of command strings the script maps over) or as older `shell` / `exec_command` function calls;
their outputs carry `exit_code`. Output size is what the model got back (`original_token_count` is the size before
Codex cut it, so it is not used).
"""

import json
import os
import re
from pathlib import Path

from .classify import READ_VERBS, SEARCH_VERBS, engine_command, read_files, shell_category
from .model import Request, Session, ToolCall, lines

STRING = re.compile(r'''"(?:[^"\\]|\\.)*"|'(?:[^'\\]|\\.)*'|`(?:[^`\\]|\\.)*`''')
KEYED = re.compile(r"""\b(?:cmd|command)["']?\s*:\s*$""")  # The text before a string that is a command argument
COMMAND_VERBS = READ_VERBS | SEARCH_VERBS | {
    "git", "python", "python3", "py", "cmake", "ninja", "ctest", "npm", "node", "pwsh", "powershell", "bash", "cmd",
    "copy-item", "move-item", "remove-item", "new-item", "set-content", "add-content", "out-file", "mkdir", "rm", "cp",
    "mv", "echo", "write-output", "start-process", "invoke-expression", "codex", "claude"}
TOOL_NAME = re.compile(r"\btools\.(\w+)\s*\(")
EXIT_CODE = re.compile(r"""\\?"exit_code\\?"\s*:\s*(-?\d+)|Exit code:\s*(-?\d+)|Process exited with code (-?\d+)""")
INJECTED = ("<environment_context", "<user_instructions", "# AGENTS.md", "<permissions", "<user_shell_command")


def home() -> Path:
    return Path(os.environ.get("CODEX_HOME") or Path.home() / ".codex")


def _meta(path: Path) -> dict:
    with path.open(encoding="utf-8", errors="replace") as file:
        for line in file:
            try:
                entry = json.loads(line)
            except ValueError:
                continue
            if entry.get("type") == "session_meta":
                return entry.get("payload") or {}
            break
    return {}


def _inside(cwd: str, root: Path) -> bool:
    if not cwd:
        return False
    normal = os.path.normcase(os.path.normpath(cwd))
    base = os.path.normcase(os.path.normpath(str(root)))
    return normal == base or normal.startswith(base.rstrip("\\/") + os.sep)


def session_files(root: Path, codex_home: Path) -> list[tuple[Path, str | None]]:
    """(log, parent thread id) for sessions whose working folder is `root` or below."""
    found = []
    for folder in ("sessions", "archived_sessions"):
        for log in (codex_home / folder).rglob("*.jsonl") if (codex_home / folder).is_dir() else []:
            meta = _meta(log)
            if _inside(meta.get("cwd", ""), root):
                found.append((log, meta.get("parent_thread_id") or _spawn_parent(meta)))
    return found


def _spawn_parent(meta: dict) -> str | None:
    source = meta.get("source")
    if isinstance(source, dict) and isinstance(source.get("subagent"), dict):
        spawn = source["subagent"].get("thread_spawn")
        return spawn.get("parent_thread_id") if isinstance(spawn, dict) else "subagent"
    return None


def _unquote(literal: str) -> str:
    if literal.startswith('"'):
        try:
            return json.loads(literal)
        except ValueError:
            pass
    return literal[1:-1].replace("\\n", "\n").replace('\\"', '"')


def _commands(script: str) -> list[str]:
    """Shell commands in an exec script: strings passed as `cmd` or `command`, and any string that starts with a
    command, such as the items of a list the script runs one by one."""
    found = []
    for match in STRING.finditer(script):
        text = _unquote(match.group(0)).strip()
        if not text:
            continue
        keyed = KEYED.search(script[max(0, match.start() - 24):match.start()])
        words = text.split(maxsplit=1)
        if keyed or (len(words) > 1 and words[0].lower() in COMMAND_VERBS):  # A lone word is a label, as "git"
            found.append(text)
    return found

def _call(name: str, script: str) -> ToolCall:
    """A tool call from an exec script or a function call's arguments."""
    commands = _commands(script)
    if not commands and name in {"shell", "exec_command", "shell_command", "local_shell"}:
        try:
            data = json.loads(script)
            command = data.get("cmd") or data.get("command") or ""
            commands = [" ".join(command) if isinstance(command, list) else str(command)]
        except ValueError:
            pass
    inner = set(TOOL_NAME.findall(script))
    if commands:
        joined = " ; ".join(commands)
        engines = [found for found in map(engine_command, commands) if found]
        files = [file for command in commands for file in read_files(command)]
        category = "engine" if engines else shell_category(commands[0])
        return ToolCall(name, category, joined, engine=engines[0] if engines else None, files=files)
    if "apply_patch" in inner or name == "apply_patch":
        return ToolCall(name, "edit", "apply_patch")
    if inner & {"web_search", "web_fetch"}:
        return ToolCall(name, "web", ",".join(sorted(inner)))
    if name in {"spawn_agent", "send_message", "wait_agent", "followup_task"}:
        return ToolCall(name, "subagent")
    if name in {"request_user_input", "request_user_input_async", "sleep", "wait", "update_plan"}:
        return ToolCall(name, "harness")
    return ToolCall(name, "other", ",".join(sorted(inner)))


def _output_text(output) -> str:
    if isinstance(output, str):
        return output
    if isinstance(output, list):
        return "".join(part.get("text", "") for part in output if isinstance(part, dict))
    return json.dumps(output) if output is not None else ""


def _request(usage: dict, model: str, effort: str = "") -> Request:
    total_input = usage.get("input_tokens", 0) or 0
    cached = usage.get("cached_input_tokens", 0) or 0
    written = usage.get("cache_write_input_tokens", 0) or 0
    return Request(input=max(total_input - cached - written, 0), cache_read=cached, cache_write=written,
                   output=usage.get("output_tokens", 0) or 0, reasoning=usage.get("reasoning_output_tokens", 0) or 0,
                   model=model, effort=effort)


def parse(path: Path, parent: str | None = None) -> Session:
    meta = _meta(path)
    session = Session("codex", meta.get("id") or path.stem, path, cwd=meta.get("cwd", ""), parent=parent)
    model = effort = ""
    records: dict[str, Request] = {}
    counted: list[Request] = []  # From token_count events, for logs without token_usage_record
    last_total = None
    calls: dict[str, ToolCall] = {}
    typed: list[str] = []  # user_message events
    items: list[str] = []  # User messages in the model's history, for logs without those events
    for line in lines(path):
        try:
            entry = json.loads(line)
        except ValueError:
            continue
        kind, payload = entry.get("type"), entry.get("payload") or {}
        if entry.get("timestamp"):
            session.started = session.started or entry["timestamp"]
            session.ended = entry["timestamp"]
        inner = payload.get("type") if isinstance(payload, dict) else None
        if kind == "turn_context":
            model = payload.get("model") or model
            effort = payload.get("effort") or effort
        elif inner == "thread_settings_applied":
            model = (payload.get("thread_settings") or {}).get("model") or model
        elif kind == "token_usage_record":
            records[payload.get("response_id") or str(len(records))] = _request(payload.get("usage") or {}, model,
                                                                                 effort)
        elif inner == "token_count":
            info = payload.get("info") or {}
            total = json.dumps(info.get("total_token_usage"), sort_keys=True)
            if info.get("last_token_usage") and total != last_total:
                counted.append(_request(info["last_token_usage"], model, effort))
                last_total = total
        elif kind == "compacted" or inner == "context_compacted":
            session.compactions += 1
        elif inner in {"custom_tool_call", "function_call", "local_shell_call"}:
            script = payload.get("input") or payload.get("arguments") or json.dumps(payload.get("action") or {})
            call = _call(payload.get("name") or inner, script if isinstance(script, str) else json.dumps(script))
            calls[payload.get("call_id", str(len(calls)))] = call
            session.tools.append(call)
        elif inner in {"custom_tool_call_output", "function_call_output", "local_shell_call_output"}:
            call = calls.get(payload.get("call_id", ""))
            if call is not None:
                text = _output_text(payload.get("output"))
                call.chars = len(text)
                codes = [int(next(group for group in match.groups() if group)) for match in EXIT_CODE.finditer(text)]
                call.failed = any(code != 0 for code in codes)
        elif inner == "user_message":
            typed.append(str(payload.get("message", "")).strip())
        elif inner == "message" and payload.get("role") == "user":
            items.append(_output_text(payload.get("content")).strip())
    prompts = [text for text in (typed or items) if text and not text.startswith(INJECTED)]
    session.prompts = len(prompts)
    session.requests = list(records.values()) if records else counted
    session.title = " ".join(prompts[0].split())[:80] if prompts else ""
    return session

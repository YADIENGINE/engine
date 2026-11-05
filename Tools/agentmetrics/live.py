"""One short line per thing an agent does, from the event stream of `codex exec --json` or `claude -p
--output-format stream-json`: commands with their exit codes, edits, messages, the end of a turn."""

import json
import re

WIDTH = 140
SHELL_WRAPPER = re.compile(r"""^"?[^"]*?(?:powershell|pwsh|bash|cmd)(?:\.exe)?"?\s+(?:-\w+\s+)*""", re.IGNORECASE)
ATTEMPT_FOLDER = re.compile(r"""^.*?engine-agent-eval[\\/][^\\/]+[\\/][^\\/]+[\\/]""")


def _command(text: str) -> str:
    """The command without the shell that wraps it: `"...powershell.exe" -Command 'x'` gives `x`."""
    stripped = SHELL_WRAPPER.sub("", text.strip())
    if len(stripped) >= 2 and stripped[0] == stripped[-1] and stripped[0] in "'\"":
        stripped = stripped[1:-1]
    return stripped


def _path(path: str) -> str:
    return ATTEMPT_FOLDER.sub("", path).replace("\\", "/")


def _short(text: str, width: int = WIDTH) -> str:
    text = " ".join(str(text).split())
    return text if len(text) <= width else text[: width - 3] + "..."


def _codex(event: dict) -> str | None:
    kind = event.get("type")
    item = event.get("item") or {}
    if kind == "item.completed":
        match item.get("type"):
            case "command_execution":
                return f"$ {_short(_command(item.get('command', '')), 110)}  -> exit {item.get('exit_code')}"
            case "file_change":
                return "edit " + ", ".join(_path(change.get("path", "?")) for change in item.get("changes", []))
            case "agent_message":
                return "says: " + _short(item.get("text", ""))
            case "web_search":
                return "web: " + _short(item.get("query", ""))
            case "error":
                return "error: " + _short(item.get("message", ""))
        return None
    if kind == "turn.completed":
        usage = event.get("usage") or {}
        return f"turn done: input {usage.get('input_tokens', 0):,} (cached {usage.get('cached_input_tokens', 0):,}), " \
               f"output {usage.get('output_tokens', 0):,}"
    if kind in ("turn.failed", "error"):
        return "FAILED: " + _short(json.dumps(event.get("error") or event.get("message") or event))
    return None


def _claude(event: dict) -> str | None:
    kind = event.get("type")
    if kind == "assistant":
        lines = []
        for block in (event.get("message") or {}).get("content") or []:
            if block.get("type") == "tool_use":
                data = block.get("input") or {}
                target = data.get("command") or data.get("file_path") or data.get("pattern") or data.get("path") or ""
                lines.append(f"{block.get('name')}: {_short(_path(str(target)), 110)}")
            elif block.get("type") == "text" and block.get("text", "").strip():
                lines.append("says: " + _short(block["text"]))
        return "\n".join(lines) or None
    if kind == "user":
        for block in (event.get("message") or {}).get("content") or []:
            if isinstance(block, dict) and block.get("type") == "tool_result" and block.get("is_error"):
                content = block.get("content")
                text = content if isinstance(content, str) else json.dumps(content)
                return "  tool failed: " + _short(text, 110)
        return None
    if kind == "result":
        return f"done: {event.get('subtype')}, {event.get('num_turns')} turns"
    return None


def describe(harness: str, line: str) -> str | None:
    """The progress line for one line of the agent's stdout; None for events not worth a line."""
    try:
        event = json.loads(line)
    except ValueError:
        return None
    if not isinstance(event, dict):
        return None
    return _codex(event) if harness == "codex" else _claude(event)

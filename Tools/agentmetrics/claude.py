"""Claude Code session logs: `<home>/projects/<encoded folder>/<session>.jsonl`, subagents under
`<session>/subagents/*.jsonl`.

Claude Code writes one line per content block of a response and repeats the response's usage on each, so requests are
counted once per message id with the last usage seen.
"""

import json
import os
import re
from pathlib import Path

from .classify import claude_category, engine_command, read_files, search_roots, shell_category
from .model import Request, Session, ToolCall, lines

KEEP_TYPES = ('"type":"assistant"', '"type":"user"', '"type":"system"', '"type":"ai-title"')
TAG = re.compile(r"<[^>]+>")


def home() -> Path:
    return Path(os.environ.get("CLAUDE_CONFIG_DIR") or Path.home() / ".claude")


def encode(folder: Path) -> str:
    """The projects/ folder name Claude Code uses for sessions started in `folder`."""
    return re.sub(r"[^A-Za-z0-9]", "-", str(folder))


def session_files(root: Path, claude_home: Path) -> list[tuple[Path, str | None]]:
    """(log, parent session id) for sessions started in `root` or below; subagent logs carry their parent."""
    projects = claude_home / "projects"
    if not projects.is_dir():
        return []
    prefix = encode(root).lower()
    found = []
    for folder in projects.iterdir():
        name = folder.name.lower()
        if not folder.is_dir() or not (name == prefix or name.startswith(prefix + "-")):
            continue
        for log in folder.glob("*.jsonl"):
            found.append((log, None))
            for sub in (folder / log.stem / "subagents").glob("*.jsonl"):
                found.append((sub, log.stem))
    return found


def _text(content) -> str:
    if isinstance(content, str):
        return content
    if isinstance(content, list):
        return "".join(part.get("text", "") for part in content if isinstance(part, dict) and part.get("type") == "text")
    return ""


def _result_size(content) -> tuple[int, int]:
    """Characters and images of a tool result."""
    if isinstance(content, str):
        return len(content), 0
    chars = images = 0
    for part in content or []:
        if isinstance(part, dict):
            if part.get("type") == "image":
                images += 1
            else:
                chars += len(part.get("text", "") or "")
    return chars, images


def _tool(name: str, data: dict) -> ToolCall:
    if name in {"Bash", "PowerShell"}:
        command = str(data.get("command", ""))
        return ToolCall(name, shell_category(command), command, engine=engine_command(command),
                        files=read_files(command), roots=search_roots(command))
    category = claude_category(name)
    if name in {"Grep", "Glob"}:
        return ToolCall(name, category, str(data.get("pattern", "")), roots=[str(data.get("path") or ".")])
    target = str(data.get("file_path") or data.get("path") or data.get("pattern") or data.get("url")
                 or data.get("description") or "")
    return ToolCall(name, category, target, files=[target] if category == "read" and target else [])


def _prompt(entry: dict) -> str | None:
    """The text of a message the user typed; None for tool results and harness messages."""
    if entry.get("isMeta") or entry.get("isCompactSummary"):
        return None
    content = (entry.get("message") or {}).get("content")
    if isinstance(content, list) and any(isinstance(part, dict) and part.get("type") == "tool_result"
                                         for part in content):
        return None
    text = _text(content).strip()
    if not text or text.startswith(("<local-command", "<command-name>", "<task-notification", "Caveat:")):
        return None
    return text


def parse(path: Path, parent: str | None = None) -> Session:
    session = Session("claude", path.stem, path, parent=parent)
    requests: dict[str, Request] = {}
    calls: dict[str, ToolCall] = {}
    first_prompt = ""
    for line in lines(path):
        if not any(marker in line for marker in KEEP_TYPES):
            continue
        try:
            entry = json.loads(line)
        except ValueError:
            continue
        if parent is None and entry.get("isSidechain"):
            continue  # Older logs kept subagent turns in the parent's file; they are not the parent's context
        kind = entry.get("type")
        if kind == "ai-title":
            session.title = entry.get("aiTitle") or session.title
            continue
        if entry.get("timestamp"):
            session.started = session.started or entry["timestamp"]
            session.ended = entry["timestamp"]
        session.cwd = session.cwd or entry.get("cwd", "")
        message = entry.get("message") or {}
        if kind == "system":
            if entry.get("subtype") == "compact_boundary":
                session.compactions += 1
        elif kind == "assistant":
            usage = message.get("usage") or {}
            key = message.get("id") or entry.get("uuid", "")
            if usage and message.get("model") != "<synthetic>":
                request = requests.setdefault(key, Request())
                request.input = usage.get("input_tokens", 0) or 0
                request.cache_read = usage.get("cache_read_input_tokens", 0) or 0
                request.cache_write = usage.get("cache_creation_input_tokens", 0) or 0
                request.output = max(request.output, usage.get("output_tokens", 0) or 0)
                request.model = message.get("model", "")
                request.effort = entry.get("effort") if isinstance(entry.get("effort"), str) else ""
            for block in message.get("content") or []:
                if isinstance(block, dict) and block.get("type") == "tool_use":
                    call = _tool(block.get("name", "?"), block.get("input") or {})
                    calls[block.get("id", str(len(calls)))] = call
                    session.tools.append(call)
        elif kind == "user":
            content = message.get("content")
            if isinstance(content, list):
                for block in content:
                    if isinstance(block, dict) and block.get("type") == "tool_result":
                        call = calls.get(block.get("tool_use_id", ""))
                        if call is not None:
                            call.chars, call.images = _result_size(block.get("content"))
                            call.failed = bool(block.get("is_error"))
            prompt = _prompt(entry)
            if prompt is not None:
                session.prompts += 1
                first_prompt = first_prompt or prompt
    session.requests = list(requests.values())
    if not session.title:
        session.title = " ".join(TAG.sub(" ", first_prompt).split())[:80]
    return session

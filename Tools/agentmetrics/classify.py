"""Which kind of work a tool call was: shell commands are split into engine commands, reads, searches and the rest."""

import re

# `python Engine build`, `python . ctl status`, `python ../engine --project . sim run x.json`, on one line
ENGINE_COMMAND = re.compile(
    r"""\bpy(?:thon)?3?(?:\.exe)?[ \t]+(?:-[A-Za-z][ \t]+)*"""
    r"""(?:"(?:[^"\n]*[/\\])?[Ee]ngine"|'(?:[^'\n]*[/\\])?[Ee]ngine'|(?:[^\s"']*[/\\])?[Ee]ngine|\.)"""
    r"""[ \t]+(?:--project[ \t]+(?:"[^"\n]*"|\S+)[ \t]+)?([a-z][a-z-]*)((?:[ \t]+-\S+)*[ \t]+[a-z][a-z-]*)?""")
READ_VERBS = {"cat", "head", "tail", "sed", "awk", "less", "more", "type", "get-content", "gc", "nl", "bat"}
SEARCH_VERBS = {"grep", "rg", "egrep", "find", "ls", "dir", "tree", "get-childitem", "gci", "select-string", "sls",
                "wc", "fd", "where", "where.exe", "which", "test-path"}
SEGMENT_SPLIT = re.compile(r"\s*(?:&&|\|\||;|\|)\s*")
FILE_TOKEN = re.compile(r"""(?:"([^"]+\.[A-Za-z0-9]{1,6})"|'([^']+\.[A-Za-z0-9]{1,6})'|([^\s"'|;&<>]+\.[A-Za-z0-9]{1,6}))""")
NOT_FILES = re.compile(r"^(?:-|\d|https?:|\$)|[*?{}]")

CLAUDE_TOOLS = {
    "Read": "read", "NotebookRead": "read",
    "Grep": "search", "Glob": "search", "LS": "search",
    "Edit": "edit", "MultiEdit": "edit", "Write": "edit", "NotebookEdit": "edit",
    "Agent": "subagent", "Task": "subagent", "SendMessage": "subagent",
    "WebFetch": "web", "WebSearch": "web",
    "TodoWrite": "harness", "ToolSearch": "harness", "Skill": "harness", "AskUserQuestion": "harness",
    "ExitPlanMode": "harness", "EnterPlanMode": "harness", "ScheduleWakeup": "harness",
}


def _known() -> dict[str, set[str]]:
    """Engine commands and, for those with actions, their actions; read from the command line parser."""
    global _COMMANDS
    if _COMMANDS is None:
        from ..cli import build_parser  # Imported late: the parser imports this package's command

        _COMMANDS = {}
        commands = next(action for action in build_parser()._actions if action.dest == "command")
        for name, parser in commands.choices.items():
            nested = [action for action in parser._actions if getattr(action, "choices", None)
                      and action.dest == "action"]
            _COMMANDS[name] = set(nested[0].choices) if nested else set()
    return _COMMANDS


_COMMANDS: dict[str, set[str]] | None = None


def engine_command(command: str) -> str | None:
    """The engine command a shell command runs, e.g. "build" or "ctl screenshot"; None when it runs none."""
    known = _known()
    for match in ENGINE_COMMAND.finditer(command):
        name = match.group(1)
        if name not in known:
            continue
        action = (match.group(2) or "").split()[-1:] or [""]
        return f"{name} {action[0]}" if action[0] in known[name] else name
    return None


def _segments(command: str) -> list[list[str]]:
    words = []
    for segment in SEGMENT_SPLIT.split(command):
        parts = segment.strip().split()
        while parts and (parts[0] in {"cd", "Set-Location", "pushd"} or "=" in parts[0].split("/")[0]):
            parts = parts[2:] if parts[0] in {"cd", "Set-Location", "pushd"} else parts[1:]
        if parts:
            words.append(parts)
    return words


def read_files(command: str) -> list[str]:
    """Files a shell command prints: arguments of cat, sed, head, Get-Content and the like."""
    files = []
    for segment in SEGMENT_SPLIT.split(command):
        parts = segment.strip().split(maxsplit=1)
        if len(parts) < 2 or parts[0].lower().split("\\")[-1].split("/")[-1] not in READ_VERBS:
            continue
        for match in FILE_TOKEN.finditer(parts[1]):
            token = next(group for group in match.groups() if group)
            if not NOT_FILES.search(token):
                files.append(token)
    return files


def shell_category(command: str) -> str:
    """engine, read, search, git, build-tools or shell, by the first command that is not a `cd`."""
    if engine_command(command):
        return "engine"
    segments = _segments(command)
    if not segments:
        return "shell"
    verb = segments[0][0].lower().strip("&").split("\\")[-1].split("/")[-1]
    if verb in READ_VERBS or read_files(command):
        return "read"
    if verb in SEARCH_VERBS:
        return "search"
    if verb == "git":
        return "git"
    if verb in {"cmake", "ninja", "ctest", "msbuild", "cl", "cl.exe"}:
        return "build-tools"
    return "shell"


def claude_category(name: str) -> str:
    if name.startswith("mcp__"):
        return "mcp"
    return CLAUDE_TOOLS.get(name, "other")


def area(path: str) -> str:
    """Where a read file lives: AgentDocs, engine docs, engine source, game code, or elsewhere."""
    normal = path.replace("\\", "/")
    for marker, name in (("AgentDocs/", "AgentDocs"), ("/build/", "build output"), ("/.claude/", "agent files"),
                         ("/.codex/", "agent files"), ("/Game/", "game"), ("Docs/", "engine Docs"),
                         ("/Core/", "engine source"), ("/Assets/", "engine source"), ("/Editor/", "editor source"),
                         ("/Tools/", "engine tools")):
        if marker in normal or normal.startswith(marker.lstrip("/")):
            return name
    if normal.rsplit("/", 1)[-1] in {"CLAUDE.md", "AGENTS.md"}:
        return "agent files"
    return "other"

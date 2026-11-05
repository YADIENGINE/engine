"""`agent-report`: what coding agents spent on this project, read from the logs Claude Code and Codex already write.

Sessions count when they started in the project root or below. Nothing is sent anywhere and nothing is written; the
report is printed (text, or JSON with --json). Docs/Contributing/AgentMetrics.md describes every number.
"""

import json
from datetime import date
from pathlib import Path

from ..agentmetrics import claude, codex
from ..agentmetrics.model import Session
from ..agentmetrics.report import render, summarize
from ..errors import EXIT_INFRA, EXIT_OK, ToolError
from ..manifest import ENGINE_ROOT

HARNESSES = ("claude", "codex")


def add_parser(commands) -> None:
    command = commands.add_parser("agent-report", help="tokens coding agents spent on this project, from their logs")
    command.add_argument("--harness", choices=("all", *HARNESSES), default="all")
    command.add_argument("--since", type=date.fromisoformat, metavar="YYYY-MM-DD", help="sessions started that day "
                         "or later")
    command.add_argument("--until", type=date.fromisoformat, metavar="YYYY-MM-DD", help="sessions started that day "
                         "or earlier")
    command.add_argument("--session", metavar="ID", help="only sessions whose id starts with ID, with their subagents")
    command.add_argument("--no-subagents", action="store_true", help="leave subagent sessions out")
    command.add_argument("--top", type=int, default=10, metavar="N", help="rows in each list (default 10)")
    command.add_argument("--json", action="store_true", help="the whole summary as JSON")
    command.add_argument("--claude-home", type=Path, help="Claude Code folder (default: CLAUDE_CONFIG_DIR or ~/.claude)")
    command.add_argument("--codex-home", type=Path, help="Codex folder (default: CODEX_HOME or ~/.codex)")


def load_sessions(root: Path, harness: str = "all", claude_home: Path | None = None,
                  codex_home: Path | None = None) -> list[Session]:
    sessions = []
    if harness in ("all", "claude"):
        for log, parent in claude.session_files(root, claude_home or claude.home()):
            sessions.append(claude.parse(log, parent))
    if harness in ("all", "codex"):
        for log, parent in codex.session_files(root, codex_home or codex.home()):
            sessions.append(codex.parse(log, parent))
    return sessions


def select(sessions: list[Session], since: date | None = None, until: date | None = None,
           session: str | None = None, subagents: bool = True) -> list[Session]:
    def day(found: Session) -> date | None:
        try:
            return date.fromisoformat(found.started[:10])
        except ValueError:
            return None

    chosen = []
    for found in sessions:
        started = day(found)
        if since and (started is None or started < since) or until and (started is None or started > until):
            continue
        if found.subagent and not subagents:
            continue
        if session and not (found.id.startswith(session) or (found.parent or "").startswith(session)):
            continue
        if found.requests or found.tools:
            chosen.append(found)
    return chosen


def agent_report(root: Path, args) -> int:
    sessions = select(load_sessions(root, args.harness, args.claude_home, args.codex_home), args.since, args.until,
                      args.session, not args.no_subagents)
    if not sessions:
        raise ToolError(f"No agent sessions found for {root}", EXIT_INFRA)
    summary = summarize(sessions, root, ENGINE_ROOT, max(1, args.top))
    print(json.dumps(summary, indent=2) if args.json else render(summary))
    return EXIT_OK

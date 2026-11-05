"""Summaries of sessions: a JSON-ready dictionary, and the same as text for a terminal."""

from collections import Counter, defaultdict
from pathlib import Path

from .classify import area
from .model import Session, ToolCall

TOKEN_FIELDS = ("input", "cache_read", "cache_write", "output", "reasoning")
LARGE_CONTEXT = (100_000, 200_000, 400_000)
REPEATED_READS = 3


def _percentile(values: list[int], share: float) -> int:
    if not values:
        return 0
    ordered = sorted(values)
    return ordered[min(len(ordered) - 1, int(len(ordered) * share))]


def _normal(path: str, root: Path) -> str:
    """A file path relative to the project when it lies inside it, with forward slashes."""
    normal = path.replace("\\", "/")
    if len(normal) > 3 and normal[0] == "/" and normal[2] == "/" and normal[1].isalpha():
        normal = f"{normal[1].upper()}:{normal[2:]}"  # Git Bash /c/... paths
    base = str(root).replace("\\", "/").rstrip("/") + "/"
    return normal[len(base):] if normal.lower().startswith(base.lower()) else normal


def _failure_runs(tools: list[ToolCall]) -> int:
    """Longest run of failed engine commands one after another (a build-fix loop)."""
    longest = current = 0
    for call in tools:
        if call.engine is None:
            continue
        current = current + 1 if call.failed else 0
        longest = max(longest, current)
    return longest


def session_row(session: Session) -> dict:
    return {
        "harness": session.harness,
        "id": session.id,
        "parent": session.parent,
        "started": session.started,
        "ended": session.ended,
        "title": session.title,
        "models": session.models,
        "efforts": session.efforts,
        "prompts": session.prompts,
        "requests": len(session.requests),
        "toolCalls": len(session.tools),
        "compactions": session.compactions,
        "tokens": {name: session.sum(name) for name in TOKEN_FIELDS} | {
            "context": session.sum("context"), "total": session.sum("total")},
        "context": {"startup": session.startup_context,
                    "median": _percentile([request.context for request in session.requests], 0.5),
                    "peak": session.peak_context},
        "toolOutputTokens": sum(call.output_tokens for call in session.tools),
        "engineFailures": sum(1 for call in session.tools if call.engine and call.failed),
        "longestFailureRun": _failure_runs(session.tools),
    }


def summarize(sessions: list[Session], root: Path, engine_root: Path, top: int = 10) -> dict:
    """Everything the report shows, as plain data."""
    requests = [request for session in sessions for request in session.requests]
    contexts = [request.context for request in requests]
    tools = [call for session in sessions for call in session.tools]
    main = [session for session in sessions if not session.subagent]

    categories: dict[str, dict] = defaultdict(lambda: {"calls": 0, "tokens": 0, "failed": 0, "largest": 0})
    for call in tools:
        entry = categories[call.category]
        entry["calls"] += 1
        entry["tokens"] += call.output_tokens
        entry["failed"] += call.failed
        entry["largest"] = max(entry["largest"], call.output_tokens)

    engine: dict[str, dict] = defaultdict(lambda: {"runs": 0, "failed": 0, "tokens": 0, "largest": 0})
    for call in tools:
        if call.engine:
            entry = engine[call.engine]
            entry["runs"] += 1
            entry["failed"] += call.failed
            entry["tokens"] += call.output_tokens
            entry["largest"] = max(entry["largest"], call.output_tokens)

    reads: dict[str, dict] = defaultdict(lambda: {"reads": 0, "tokens": 0})
    areas: Counter = Counter()
    for call in tools:
        if not call.files:
            continue
        share = call.output_tokens // len(call.files)  # One call printing several files: split its output evenly
        for file in call.files:
            name = _normal(file, root)
            reads[name]["reads"] += 1
            reads[name]["tokens"] += share
            areas[area(name if "/" in name else "/" + name)] += share
    repeated = {name: entry for name, entry in reads.items() if entry["reads"] >= REPEATED_READS}

    skills_dir = engine_root / "AgentDocs" / "Skills"
    skills = sorted(path.parent.name for path in skills_dir.glob("*/SKILL.md")) if skills_dir.is_dir() else []
    agent_docs: Counter = Counter()
    for name, entry in reads.items():
        if "AgentDocs/" in name:
            agent_docs[name.split("AgentDocs/", 1)[1]] += entry["reads"]  # Same file from any folder or worktree
    skills_read = {skill for skill in skills if f"Skills/{skill}/SKILL.md" in agent_docs}

    largest = sorted(tools, key=lambda call: call.output_tokens, reverse=True)[:top]
    rows = sorted((session_row(session) for session in sessions), key=lambda row: row["tokens"]["total"],
                  reverse=True)
    return {
        "root": str(root),
        "top": top,
        "sessions": {"main": len(main), "subagent": len(sessions) - len(main),
                     "harnesses": dict(Counter(session.harness for session in sessions)),
                     "first": min((s.started for s in sessions if s.started), default=""),
                     "last": max((s.ended for s in sessions if s.ended), default="")},
        "models": dict(Counter(request.model for request in requests if request.model).most_common()),
        "counts": {"prompts": sum(s.prompts for s in main), "requests": len(requests), "toolCalls": len(tools),
                   "compactions": sum(s.compactions for s in sessions)},
        "tokens": {name: sum(getattr(r, name) for r in requests) for name in TOKEN_FIELDS} | {
            "context": sum(contexts), "total": sum(r.total for r in requests)},
        "perSession": {"medianTotal": _percentile([s.sum("total") for s in main], 0.5),
                       "medianRequests": _percentile([len(s.requests) for s in main], 0.5),
                       "medianStartupContext": _percentile([s.startup_context for s in main if s.requests], 0.5)},
        "context": {"median": _percentile(contexts, 0.5), "p90": _percentile(contexts, 0.9),
                    "peak": max(contexts, default=0),
                    "requestsAbove": {str(limit): sum(1 for c in contexts if c > limit) for limit in LARGE_CONTEXT}},
        "toolOutput": {"tokens": sum(call.output_tokens for call in tools),
                       "images": sum(call.images for call in tools),
                       "byCategory": dict(sorted(categories.items(), key=lambda item: -item[1]["tokens"]))},
        "engineCommands": dict(sorted(engine.items(), key=lambda item: -item[1]["tokens"])),
        "failureLoops": {"engineFailures": sum(e["failed"] for e in engine.values()),
                         "longestRun": max((_failure_runs(s.tools) for s in sessions), default=0)},
        "reads": {"files": len(reads), "tokens": sum(e["tokens"] for e in reads.values()),
                  "byArea": dict(areas.most_common()),
                  "top": dict(sorted(reads.items(), key=lambda item: -item[1]["tokens"])[:top]),
                  "repeated": {"files": len(repeated), "extraReads": sum(e["reads"] - 1 for e in repeated.values()),
                               "top": dict(sorted(repeated.items(), key=lambda item: -item[1]["reads"])[:top])}},
        "agentDocs": {"files": len(agent_docs), "reads": sum(agent_docs.values()),
                      "read": dict(agent_docs.most_common()),
                      "skillsNeverRead": [skill for skill in skills if skill not in skills_read]},
        "largestOutputs": [{"tool": call.name, "category": call.category, "tokens": call.output_tokens,
                            "target": " ".join(call.target.split())[:160]} for call in largest],
        "topSessions": rows[:top],
    }


def _k(value: int) -> str:
    if value >= 10_000_000:
        return f"{value / 1_000_000:.0f}M"
    if value >= 1_000_000:
        return f"{value / 1_000_000:.1f}M"
    if value >= 10_000:
        return f"{value / 1000:.0f}K"
    if value >= 1000:
        return f"{value / 1000:.1f}K"
    return str(value)


def _share(part: int, whole: int) -> str:
    return f"{100 * part / whole:.0f}%" if whole else "-"


def render(summary: dict) -> str:
    """The summary as text. Tool output tokens are estimated from characters where the harness gives no count."""
    lines: list[str] = []
    add = lines.append
    sessions, counts, tokens = summary["sessions"], summary["counts"], summary["tokens"]
    harnesses = ", ".join(f"{name} {count}" for name, count in sessions["harnesses"].items()) or "none"
    add(f"Agent sessions in {summary['root']}")
    add(f"  {sessions['main']} sessions + {sessions['subagent']} subagent ({harnesses}); "
        f"{sessions['first'][:10]} .. {sessions['last'][:10]}")
    add(f"  {counts['prompts']} prompts, {counts['requests']} model requests, {counts['toolCalls']} tool calls, "
        f"{counts['compactions']} compactions")
    if summary["models"]:
        add("  models: " + ", ".join(f"{name} ({count})" for name, count in summary["models"].items()))
    if not counts["requests"]:
        return "\n".join(lines)

    total = tokens["total"]
    add("")
    add("Tokens")
    add(f"  total {_k(total)} = context {_k(tokens['context'])} + output {_k(tokens['output'])}")
    add(f"  context: cache read {_k(tokens['cache_read'])} ({_share(tokens['cache_read'], total)}), "
        f"cache write {_k(tokens['cache_write'])} ({_share(tokens['cache_write'], total)}), "
        f"uncached {_k(tokens['input'])} ({_share(tokens['input'], total)})")
    if tokens["reasoning"]:
        add(f"  output includes reasoning {_k(tokens['reasoning'])}")
    per = summary["perSession"]
    add(f"  per session (median): {_k(per['medianTotal'])} tokens, {per['medianRequests']} requests, "
        f"startup context {_k(per['medianStartupContext'])}")
    context = summary["context"]
    above = ", ".join(f">{_k(int(limit))}: {count}" for limit, count in context["requestsAbove"].items())
    add(f"  context per request: median {_k(context['median'])}, p90 {_k(context['p90'])}, "
        f"peak {_k(context['peak'])}; requests {above}")

    output = summary["toolOutput"]
    add("")
    add(f"Tool output (~{_k(output['tokens'])} tokens when first read; {output['images']} images)")
    add(f"  {'category':14} {'calls':>6} {'tokens':>8} {'share':>6} {'avg':>7} {'largest':>8} {'failed':>7}")
    for name, entry in output["byCategory"].items():
        add(f"  {name:14} {entry['calls']:>6} {_k(entry['tokens']):>8} {_share(entry['tokens'], output['tokens']):>6} "
            f"{_k(entry['tokens'] // max(1, entry['calls'])):>7} {_k(entry['largest']):>8} {entry['failed']:>7}")

    if summary["engineCommands"]:
        add("")
        loops = summary["failureLoops"]
        add(f"Engine commands ({loops['engineFailures']} failed; longest run of failures in a row: "
            f"{loops['longestRun']})")
        add(f"  {'command':22} {'runs':>5} {'failed':>7} {'tokens':>8} {'avg':>7} {'largest':>8}")
        for name, entry in summary["engineCommands"].items():
            add(f"  {name:22} {entry['runs']:>5} {entry['failed']:>7} {_k(entry['tokens']):>8} "
                f"{_k(entry['tokens'] // max(1, entry['runs'])):>7} {_k(entry['largest']):>8}")

    reads = summary["reads"]
    if reads["files"]:
        add("")
        add(f"File reads: {reads['files']} files, ~{_k(reads['tokens'])} tokens")
        add("  by area: " + ", ".join(f"{name} {_k(value)}" for name, value in reads["byArea"].items()))
        add("  largest:")
        for name, entry in reads["top"].items():
            add(f"    {_k(entry['tokens']):>7} {entry['reads']:>4}x  {name}")
        repeated = reads["repeated"]
        if repeated["files"]:
            add(f"  read {REPEATED_READS}+ times: {repeated['files']} files, {repeated['extraReads']} repeat reads")
            for name, entry in repeated["top"].items():
                add(f"    {entry['reads']:>4}x {_k(entry['tokens']):>7}  {name}")

    docs = summary["agentDocs"]
    add("")
    add("AgentDocs")
    if docs["read"]:
        shown = list(docs["read"].items())[:summary["top"]]
        add(f"  {docs['files']} files read {docs['reads']} times; most: "
            + ", ".join(f"{name} ({count})" for name, count in shown))
    else:
        add("  read: none")
    if docs["skillsNeverRead"]:
        add("  skills never read: " + ", ".join(docs["skillsNeverRead"]))

    add("")
    add("Largest tool outputs")
    for entry in summary["largestOutputs"]:
        add(f"  {_k(entry['tokens']):>7}  {entry['tool']} ({entry['category']}): {entry['target'][:100]}")

    add("")
    add("Sessions by tokens")
    add(f"  {'started':16} {'harness':7} {'id':8} {'req':>5} {'peak':>6} {'total':>7} {'output':>7} {'fails':>5}  title")
    for row in summary["topSessions"]:
        marker = "  sub " if row["parent"] else ""
        add(f"  {row['started'][:16].replace('T', ' '):16} {row['harness']:7} {row['id'][:8]:8} {row['requests']:>5} "
            f"{_k(row['context']['peak']):>6} {_k(row['tokens']['total']):>7} {_k(row['tokens']['output']):>7} "
            f"{row['engineFailures']:>5}  {marker}{row['title'][:60]}")
    return "\n".join(lines)

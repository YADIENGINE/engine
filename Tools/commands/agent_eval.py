"""`agent-eval`: fixed tasks for coding agents on a fixture project; how many pass and how many tokens they take.

Engine contributors run it when a change touches what agents read or run (AgentDocs, tool output, API names). Results
stay under build/agent-eval/; `baseline` copies a run's medians to Tools/AgentEval/Baseline.json, which is committed.
Docs/Contributing/AgentMetrics.md.
"""

import json
import time

from ..agentmetrics import evaluation
from ..agentmetrics.live import describe
from ..agentmetrics.report import render_behaviour
from ..errors import EXIT_FAILED, EXIT_INFRA, EXIT_OK, ToolError

COLUMNS = ("medianTotal", "medianOutput", "medianRequests", "medianToolOutput", "medianPeakContext")


def add_parser(commands) -> None:
    command = commands.add_parser("agent-eval", help="run fixed agent tasks on a fixture project and measure tokens")
    actions = command.add_subparsers(dest="action", required=True, metavar="ACTION")
    actions.add_parser("list", help="the tasks, their status and checks")

    def selection(parser) -> None:
        parser.add_argument("--task", action="append", metavar="ID", help="only this task (repeatable); drafts too")
        parser.add_argument("--all", action="store_true", help="include draft tasks")
        parser.add_argument("--smoke", action="store_true", help="only the quick subset")
        parser.add_argument("--keep", action="store_true", help="keep every attempt's project, not only failed ones")

    parser = actions.add_parser("verify", help="without an agent: each task fails on the fixture and passes with its "
                                "solution")
    selection(parser)
    parser = actions.add_parser("run", help="run the tasks with an agent; results under build/agent-eval/runs")
    selection(parser)
    parser.add_argument("--harness", choices=evaluation.HARNESSES, default="codex")
    parser.add_argument("--effort", choices=tuple(evaluation.EFFORTS), default=evaluation.DEFAULT_EFFORT,
                        help="m: medium (default), h: high; the model is pinned per harness "
                        f"({', '.join(f'{h} {m}' for h, m in evaluation.MODELS.items())})")
    parser.add_argument("--repeat", type=int, default=3, metavar="N", help="attempts per task (default 3; --smoke: 1)")
    parser = actions.add_parser("watch", help="follow a running run from another terminal: each attempt's commands, "
                                "edits and messages as they happen")
    parser.add_argument("run", nargs="?", metavar="RUN", help="default: the latest run")
    parser = actions.add_parser("show", help="a run's summary (default: the latest)")
    parser.add_argument("run", nargs="?", metavar="RUN")
    parser.add_argument("--json", action="store_true")
    parser = actions.add_parser("compare", help="a run against the baseline of its harness, or against another run")
    parser.add_argument("run", nargs="?", metavar="RUN", help="default: the latest run")
    parser.add_argument("--against", metavar="RUN", help="another run instead of the baseline")
    parser = actions.add_parser("baseline", help="make a run the committed baseline of its harness")
    parser.add_argument("run", nargs="?", metavar="RUN", help="default: the latest run")


def _selected(args) -> list[evaluation.Task]:
    tasks = evaluation.load_tasks(args.task)
    if not args.task and not args.all:
        tasks = [task for task in tasks if task.status == "ready"]
    if args.smoke:
        tasks = [task for task in tasks if task.smoke]
    return tasks


def _k(value) -> str:
    if value is None:
        return "-"
    value = float(value)
    if value >= 1_000_000:
        return f"{value / 1_000_000:.2f}M"
    if value >= 1000:
        return f"{value / 1000:.0f}K"
    return f"{value:.0f}"


def render_summary(summary: dict) -> str:
    agent = (f"{summary['harness']} {summary.get('model')}, effort {summary.get('effort')}" if summary.get("harness")
             else "no agent")
    lines = [f"Run {summary['run']}: {agent}; engine "
             f"{summary['engine']['commit']}{' (uncommitted changes)' if summary['engine']['dirty'] else ''}"]
    seen = [name for name in summary.get("models") or [] if name != summary.get("model")
            and not name.startswith("codex-auto-review")]  # Codex's own approval reviewer is not the agent
    if seen:
        lines.append(f"  warning: the logs also name {', '.join(seen)}, not only the pinned model")
    efforts = [name for name in summary.get("efforts") or [] if name != summary.get("effort")]
    if efforts:
        lines.append(f"  warning: the logs show effort {', '.join(efforts)}, not only {summary.get('effort')}")
    overall = summary["overall"]
    lines.append(f"  {overall['passed']}/{overall['attempts']} attempts passed; {_k(overall['totalTokens'])} tokens; "
                 f"{_k(overall['tokensPerSuccess'])} tokens per success")
    if overall.get("errors"):
        lines.append(f"  {overall['errors']} attempt(s) stopped by the run's machinery, not the agent: see attempts.json")
    lines.append(f"  {'task':22} {'pass':>5} {'total':>8} {'output':>7} {'req':>5} {'tools':>7} {'peak':>7} "
                 f"{'fails':>5} {'secs':>6} {'per ok':>8}")
    for task_id, task in summary["tasks"].items():
        lines.append(f"  {task_id:22} {task['passed']}/{task['attempts']:<3} {_k(task['medianTotal']):>8} "
                     f"{_k(task['medianOutput']):>7} {_k(task['medianRequests']):>5} {_k(task['medianToolOutput']):>7} "
                     f"{_k(task['medianPeakContext']):>7} {_k(task['medianEngineFailures']):>5} "
                     f"{task['medianSeconds']:>6.0f} {_k(task['tokensPerSuccess']):>8}")
    lines.append("  (medians per attempt; tools = tool output tokens; per ok = all tokens / passed attempts)")
    if summary.get("behaviour"):
        lines.append("  Behaviour, all attempts:")
        lines.append(render_behaviour(summary["behaviour"], summary["overall"]["attempts"]).replace("\n  ", "\n    ")
                     .replace("  ", "    ", 1))
    return "\n".join(lines)


def _change(new, old) -> str:
    if not old:
        return "new" if new else "-"
    return f"{100 * (new - old) / old:+.0f}%"


def render_compare(new: dict, old: dict, label: str) -> str:
    lines = [f"{new['run']} against {label}"]
    old_tasks = old.get("tasks", {})
    lines.append(f"  {'task':22} {'pass':>9} {'total':>18} {'output':>16} {'requests':>14} {'tool output':>16}")
    for task_id, task in new["tasks"].items():
        before = old_tasks.get(task_id, {})
        cells = []
        for key in ("medianTotal", "medianOutput", "medianRequests", "medianToolOutput"):
            cells.append(f"{_k(task[key])} ({_change(task[key], before.get(key))})")
        passed = f"{task['passed']}/{task['attempts']}"
        was = f"{before['passed']}/{before['attempts']}" if before else "-"
        lines.append(f"  {task_id:22} {passed + ' (' + was + ')':>9} {cells[0]:>18} {cells[1]:>16} {cells[2]:>14} "
                     f"{cells[3]:>16}")
    o, p = new["overall"], old.get("overall", {})
    lines.append(f"  overall: {o['passed']}/{o['attempts']} passed (was {p.get('passed', '-')}/"
                 f"{p.get('attempts', '-')}); tokens per success {_k(o['tokensPerSuccess'])} "
                 f"({_change(o['tokensPerSuccess'] or 0, p.get('tokensPerSuccess'))})")
    return "\n".join(lines)


def watch(name: str | None, poll_seconds: float = 1.0) -> int:
    """Prints the progress lines of every attempt log in the run as they grow, until the run writes its summary."""
    folder = evaluation.RESULTS / "runs"
    runs = sorted(path for path in folder.glob("*") if path.is_dir()) if folder.is_dir() else []
    if name:
        runs = [path for path in runs if path.name.startswith(name)]
    if not runs:
        raise ToolError("No agent-eval run to watch", EXIT_INFRA)
    run_dir = runs[-1]
    print(f"Watching {run_dir.name} (Ctrl+C stops watching, not the run)", flush=True)
    offsets: dict = {}
    while True:
        for log in sorted(run_dir.glob("*.log")):
            harness = log.name.rsplit(".", 2)[-2]
            with log.open(encoding="utf-8", errors="replace") as file:
                file.seek(offsets.get(log, 0))
                while (line := file.readline()).endswith("\n"):
                    described = describe(harness, line)
                    for part in (described or "").splitlines():
                        print(f"[{log.name.split('.')[0]}] {part}", flush=True)
                    offsets[log] = file.tell()
        if (run_dir / "summary.json").is_file():
            print(f"Run finished: python Engine agent-eval show {run_dir.name}")
            return EXIT_OK
        time.sleep(poll_seconds)


def agent_eval(args) -> int:
    match args.action:
        case "list":
            for task in evaluation.load_tasks():
                checks = ", ".join(check.get("name") or check["type"] for check in task.checks)
                print(f"{task.id:22} {task.status:6} {'smoke' if task.smoke else '':5} {task.title}")
                print(f"{'':36}checks: {checks}")
            return EXIT_OK
        case "verify":
            # Each task twice without an agent: the fixture must fail its checks, the solution must pass them
            tasks = _selected(args)
            failures = []
            for solution in (False, True):
                _, summary = evaluation.run(tasks, None, 1, evaluation.DEFAULT_EFFORT, args.keep, solution,
                                            "solution" if solution else "fixture")
                for task_id, task in summary["tasks"].items():
                    if (task["passed"] == 1) != solution:
                        failures.append(f"{task_id}: {'solution fails' if solution else 'fixture already passes'}")
            for line in failures:
                print(line)
            print(f"Verify: {len(failures)} problem(s)" if failures else f"Verify: {len(tasks)} task(s) OK")
            return EXIT_FAILED if failures else EXIT_OK
        case "run":
            tasks = _selected(args)
            repeat = 1 if args.smoke and args.repeat == 3 else max(1, args.repeat)
            run_dir, summary = evaluation.run(tasks, args.harness, repeat, args.effort, args.keep,
                                              label=evaluation.setting(args.harness, args.effort)
                                              + ("-smoke" if args.smoke else ""))
            print(render_summary(summary))
            print(f"Results: {run_dir}")
            return EXIT_OK
        case "watch":
            return watch(args.run)
        case "show":
            summary = evaluation.load_summary(args.run)
            print(json.dumps(summary, indent=2) if args.json else render_summary(summary))
            return EXIT_OK
        case "compare":
            summary = evaluation.load_summary(args.run)
            if args.against:
                other = evaluation.load_summary(args.against)
                print(render_compare(summary, other, other["run"]))
            else:
                key = summary.get("setting")
                baseline = evaluation.read_baseline().get(key)
                if not baseline:
                    raise ToolError(f"No {key} baseline yet; make one with `agent-eval baseline`", EXIT_INFRA)
                print(render_compare(summary, baseline, f"the {key} baseline ({baseline['run']})"))
            return EXIT_OK
        case "baseline":
            summary = evaluation.load_summary(args.run)
            if not summary.get("harness"):
                raise ToolError("Only a run with an agent can be a baseline", EXIT_INFRA)
            evaluation.write_baseline(summary)
            print(f"{evaluation.BASELINE}: {summary['setting']} baseline is now {summary['run']}")
            return EXIT_OK
    raise AssertionError(args.action)

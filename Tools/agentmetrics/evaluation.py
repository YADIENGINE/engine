"""Fixed tasks for coding agents on a fixture project, graded by machine and measured in tokens.

Each attempt gets a fresh project outside any folder with agent files: the fixture (Tools/AgentEval/Fixture), the
template files `new` adds, and a copy of this engine's working tree as a released copy would hold it (export-ignored
files and Tools/AgentEval left out), linked to a prebuilt package of the same files so the agent's builds stay short.
The agent runs headless; afterwards the task's hidden checks are copied in and run. Docs/Contributing/AgentMetrics.md.
"""

import hashlib
import json
import math
import os
import re
import shutil
import stat
import statistics
import struct
import subprocess
import sys
import tempfile
import threading
import time
import wave
from dataclasses import dataclass, field
from datetime import datetime
from fnmatch import fnmatch
from pathlib import Path

from ..commands.new import setup_project
from ..errors import EXIT_INFRA, ToolError
from ..manifest import ENGINE_ROOT
from ..prebuilt import COPY_RECORD, PREBUILT_VARIABLE
from . import claude, codex
from .live import describe
from .model import Session
from .report import session_row

EVAL_ROOT = ENGINE_ROOT / "Tools" / "AgentEval"
FIXTURE = EVAL_ROOT / "Fixture"
TASKS = EVAL_ROOT / "Tasks"
BASELINE = EVAL_ROOT / "Baseline.json"
RESULTS = ENGINE_ROOT / "build" / "agent-eval"
WORK = Path(tempfile.gettempdir()) / "engine-agent-eval"
PROJECT_NAME = "Arena"
FONT = ("ThirdParty/NotoSans/NotoSans-Regular.ttf", "Game/Assets/Fonts/Arena.ttf")
HIT_SOUND = "Game/Assets/Audio/Hit.wav"
CHECKS_FOLDER = ".eval-checks"
HARNESSES = ("codex", "claude")
# Pinned so a change to someone's own agent settings cannot move the numbers; a run records both.
MODELS = {"codex": "gpt-5.6-sol", "claude": "claude-opus-5-5"}
EFFORTS = {"m": "medium", "h": "high"}
DEFAULT_EFFORT = "m"
CLAUDE_TOOLS = "Read,Edit,Write,Grep,Glob,Bash(python Engine:*)"
DROPPED_ENVIRONMENT = ("CLAUDECODE", "CLAUDE_CODE_", "ENGINE_EDITOR_", "CODEX_THREAD", "CODEX_SANDBOX", "CODEX_CI")
GIT_IDENTITY = ["-c", "user.name=agent-eval", "-c", "user.email=agent-eval@localhost", "-c", "core.autocrlf=false"]
TAIL_CHARS = 3000
GAME_CHECKS = ("Internals boundaries: OK", "Tuning files: OK")
COMPILED_SUFFIXES = (".h", ".hpp", ".c", ".cpp", ".inl", ".cmake", ".json", ".glsl", ".vs", ".fs", ".rc", ".ico", ".ttf",
                     ".png", ".wav")


@dataclass
class Task:
    id: str
    folder: Path
    title: str
    prompt: str
    status: str = "draft"  # "draft" until reviewed, then "ready"
    smoke: bool = False  # In the quick subset
    broken_build: bool = False  # The task's setup breaks the build on purpose
    timeout_minutes: int = 30
    checks: list[dict] = field(default_factory=list)
    setup_edits: list[dict] = field(default_factory=list)  # {"file", "old", "new"}: applied before the agent
    solution_edits: list[dict] = field(default_factory=list)  # The reference solution, for `verify`
    notes: str = ""


def load_tasks(ids: list[str] | None = None, tasks_dir: Path = TASKS) -> list[Task]:
    tasks = []
    for path in sorted(tasks_dir.glob("*/task.json")):
        data = json.loads(path.read_text(encoding="utf-8"))
        tasks.append(Task(id=path.parent.name, folder=path.parent, title=data["title"], prompt=data["prompt"],
                          status=data.get("status", "draft"), smoke=bool(data.get("smoke")),
                          broken_build=bool(data.get("brokenBuild")),
                          timeout_minutes=int(data.get("timeoutMinutes", 30)), checks=data.get("checks", []),
                          setup_edits=data.get("setupEdits", []), solution_edits=data.get("solutionEdits", []),
                          notes=data.get("notes", "")))
    if ids:
        known = {task.id for task in tasks}
        unknown = [name for name in ids if name not in known]
        if unknown:
            raise ToolError(f"Unknown task(s): {', '.join(unknown)}; see python Engine agent-eval list", EXIT_INFRA)
        tasks = [task for task in tasks if task.id in ids]
    return tasks


# The engine copy --------------------------------------------------------------------------------------------------


def _export_ignored(engine: Path) -> list[str]:
    patterns = []
    attributes = engine / ".gitattributes"
    if attributes.is_file():
        for line in attributes.read_text(encoding="utf-8").splitlines():
            parts = line.split()
            if len(parts) >= 2 and "export-ignore" in parts[1:]:
                patterns.append(parts[0].lstrip("/"))
    return patterns


def engine_files(engine: Path = ENGINE_ROOT) -> list[str]:
    """The working tree's files a released copy would hold: tracked and new files, without ignored, export-ignored
    and evaluation files. Uncommitted changes count, so a change can be measured before it is committed."""
    listed = subprocess.run(["git", "-C", str(engine), "ls-files", "-z", "--cached", "--others", "--exclude-standard"],
                            capture_output=True, check=True).stdout.decode("utf-8").split("\0")
    ignored = _export_ignored(engine) + ["Tools/AgentEval"]
    files = []
    for name in listed:
        if not name or not (engine / name).is_file():
            continue
        if any(name == pattern or name.startswith(pattern.rstrip("/") + "/") or fnmatch(name, pattern)
               for pattern in ignored):
            continue
        files.append(name)
    return sorted(files)


def copy_record(files: list[str], engine: Path = ENGINE_ROOT) -> dict:
    """The EngineCopy.json a released copy carries; with it the copy may link the prebuilt package."""
    return {"files": {name: "sha256:" + hashlib.sha256((engine / name).read_bytes()).hexdigest() for name in files}}


def record_key(record: dict) -> str:
    """Names the prebuilt package: only files that reach the compiler count, so a change to tools or docs reuses it."""
    compiled = {name: digest for name, digest in record["files"].items()
                if name.rsplit("/", 1)[-1] == "CMakeLists.txt" or name.startswith("ThirdParty/")
                or name.endswith(COMPILED_SUFFIXES)}
    return hashlib.sha256(json.dumps(compiled, sort_keys=True).encode("utf-8")).hexdigest()[:16]


def ensure_prebuilt(key: str, log: Path) -> Path:
    """A prebuilt package of these exact engine files, built once and reused by every attempt."""
    folder = RESULTS / "prebuilt" / key
    if (folder / "Prebuilt.json").is_file():
        return folder
    print(f"Building the engine package for this engine state ({key}); it is reused afterwards", flush=True)
    log.parent.mkdir(parents=True, exist_ok=True)
    with log.open("w", encoding="utf-8") as output:
        code = subprocess.run([sys.executable, str(ENGINE_ROOT), "prebuilt", "--output", str(folder)],
                              stdout=output, stderr=subprocess.STDOUT).returncode
    if code != 0:
        raise ToolError(f"prebuilt failed; see {log}", EXIT_INFRA)
    return folder


def prepare(task: Task, project: Path, files: list[str], record: dict) -> Path:
    """A fresh project for one attempt, committed in its own git repository."""
    if project.exists():
        remove(project)
    project.mkdir(parents=True)
    own_new_files(project)
    engine = project / "Engine"
    for name in files:
        target = engine / name
        target.parent.mkdir(parents=True, exist_ok=True)
        shutil.copy2(ENGINE_ROOT / name, target)
    (engine / COPY_RECORD).write_text(json.dumps(record) + "\n", encoding="utf-8")
    setup_project(engine, project, PROJECT_NAME)
    shutil.copytree(FIXTURE, project, dirs_exist_ok=True)
    font = project / FONT[1]
    font.parent.mkdir(parents=True, exist_ok=True)
    shutil.copy2(engine / FONT[0], font)
    write_hit_sound(project / HIT_SOUND)
    apply(project, task.folder / "setup", task.setup_edits)
    _git(project, "init", "-q")
    _git(project, "add", "-A")
    _git(project, *GIT_IDENTITY, "commit", "-q", "-m", "Fixture")
    return project


def apply(project: Path, overlay: Path, edits: list[dict]) -> None:
    """Copies an overlay folder over the project, then replaces text: each `old` must occur exactly once."""
    if overlay.is_dir():
        shutil.copytree(overlay, project, dirs_exist_ok=True)
    for edit in edits:
        path = project / edit["file"]
        text = path.read_text(encoding="utf-8")
        if text.count(edit["old"]) != 1:
            raise ToolError(f"{edit['file']}: the edit's text occurs {text.count(edit['old'])} times, not once",
                            EXIT_INFRA)
        path.write_text(text.replace(edit["old"], edit["new"]), encoding="utf-8", newline="\n")


def remove(folder: Path) -> None:
    """Deletes a folder, read-only files included (git marks its objects read-only on Windows)."""
    def writable_again(function, path, _error) -> None:
        os.chmod(path, stat.S_IWRITE)
        function(path)

    if not folder.exists():
        return
    if sys.version_info >= (3, 12):
        shutil.rmtree(folder, onexc=writable_again)
    else:
        shutil.rmtree(folder, onerror=writable_again)


def own_new_files(folder: Path) -> None:
    """On Windows, gives this user full access to everything created under `folder` later, by anyone. Codex's
    sandbox writes new files as its own user; without this grant they cannot be read, graded or deleted."""
    if os.name != "nt":
        return
    user = os.environ.get("USERNAME") or os.getlogin()
    done = subprocess.run(["icacls", str(folder), "/grant", f"{user}:(OI)(CI)F"], capture_output=True, text=True)
    if done.returncode != 0:
        raise ToolError(f"icacls could not grant {user} access to {folder}: {done.stdout}{done.stderr}", EXIT_INFRA)


def write_hit_sound(path: Path, rate: int = 22050, seconds: float = 0.15) -> None:
    """A short falling thump for the fixture, made here so no binary file lives in the repository."""
    path.parent.mkdir(parents=True, exist_ok=True)
    count = int(rate * seconds)
    samples = []
    for index in range(count):
        t = index / rate
        envelope = (1.0 - index / count) ** 2
        samples.append(int(26000 * envelope * math.sin(2 * math.pi * (180 - 400 * t) * t)))
    with wave.open(str(path), "wb") as output:
        output.setnchannels(1)
        output.setsampwidth(2)
        output.setframerate(rate)
        output.writeframes(struct.pack(f"<{count}h", *samples))


def _git(project: Path, *args: str) -> str:
    return subprocess.run(["git", "-C", str(project), *args], capture_output=True, text=True, check=True).stdout


def environment(prebuilt: Path) -> dict[str, str]:
    """This process's environment without the variables of an agent or editor session that started it."""
    env = {name: value for name, value in os.environ.items()
           if not any(name == prefix or name.startswith(prefix) for prefix in DROPPED_ENVIRONMENT)}
    env[PREBUILT_VARIABLE] = str(prebuilt)
    return env


def engine_command(project: Path, env: dict, *args: str, timeout: int = 1800) -> tuple[int, str]:
    try:
        done = subprocess.run([sys.executable, "Engine", *args], cwd=project, env=env, capture_output=True,
                              text=True, encoding="utf-8", errors="replace", timeout=timeout)
    except subprocess.TimeoutExpired:
        return -1, f"timed out after {timeout} s"
    return done.returncode, (done.stdout + done.stderr)[-TAIL_CHARS:]


def warm(project: Path, env: dict, broken_build: bool = False) -> None:
    """Builds the game and the simulation tool before the agent starts, so its first build is incremental and a
    broken fixture or task setup is not charged to the agent."""
    for args in (("build",), ("sim", "list")):
        code, output = engine_command(project, env, *args)
        if code != 0 and not broken_build:
            raise ToolError(f"The fixture does not pass `{' '.join(args)}` before the agent starts:\n{output}",
                            EXIT_INFRA)


# Grading ----------------------------------------------------------------------------------------------------------


def _matching(project: Path, pattern: str) -> list[Path]:
    return sorted(path for path in project.glob(pattern) if path.is_file() and "build" not in path.parts)


def run_check(check: dict, project: Path, env: dict) -> dict:
    kind = check["type"]
    name = check.get("name") or kind
    if kind == "build":
        code, output = engine_command(project, env, kind)
        return {"name": name, "passed": code == 0, "detail": output if code else ""}
    if kind == "check":  # The game's part of `check`: the engine's own AgentDocs are not the agent's work
        code, output = engine_command(project, env, kind)
        passed = code == 0 or all(line in output for line in GAME_CHECKS)
        return {"name": name, "passed": passed, "detail": "" if passed else output}
    if kind == "sim":
        code, output = engine_command(project, env, "sim", "run", f"{CHECKS_FOLDER}/{check['scenario']}")
        return {"name": name, "passed": code == 0, "detail": output if code else ""}
    if kind == "scenarios":  # Scenarios the agent wrote: enough of them, all passing, holding the required text
        exclude = set(check.get("exclude", []))
        found = [path for path in _matching(project, check["glob"]) if path.name not in exclude]
        text = {path: path.read_text(encoding="utf-8", errors="replace") for path in found}
        found = [path for path in found if all(needle in text[path] for needle in check.get("contains", []))]
        failures = []
        for path in found:
            code, output = engine_command(project, env, "sim", "run", path.relative_to(project).as_posix())
            if code != 0:
                failures.append(f"{path.name}: {output[-400:]}")
        passed = len(found) >= check.get("min", 1) and not failures
        return {"name": name, "passed": passed, "detail": f"{len(found)} matching scenario(s); " + "; ".join(failures)}
    if kind == "grep":  # Text in files: at least `min`, or at most `max` (0 forbids it)
        pattern = re.compile(check["pattern"], re.MULTILINE)
        hits = []
        for path in _matching(project, check["glob"]):
            count = len(pattern.findall(path.read_text(encoding="utf-8", errors="replace")))
            if count:
                hits.append(f"{path.relative_to(project).as_posix()} ({count})")
        total = sum(int(hit.rsplit("(", 1)[1].rstrip(")")) for hit in hits)
        passed = total >= check.get("min", 1) if "max" not in check else total <= check["max"]
        return {"name": name, "passed": passed, "detail": f"{total} match(es): {', '.join(hits)[:400]}"}
    if kind == "python":
        done = subprocess.run([sys.executable, "-I", str(project / CHECKS_FOLDER / check["script"]), str(project)],
                              cwd=project, env=env, capture_output=True, text=True, timeout=600)
        return {"name": name, "passed": done.returncode == 0, "detail": (done.stdout + done.stderr)[-TAIL_CHARS:]}
    raise ToolError(f"Unknown check type {kind!r}", EXIT_INFRA)


def grade(task: Task, project: Path, env: dict) -> list[dict]:
    checks_dir = project / CHECKS_FOLDER
    if (task.folder / "checks").is_dir():
        shutil.copytree(task.folder / "checks", checks_dir, dirs_exist_ok=True)
    try:
        return [run_check(check, project, env) for check in task.checks]
    finally:
        shutil.rmtree(checks_dir, ignore_errors=True)


def diff_stat(project: Path) -> dict:
    """What the attempt changed, against the fixture commit; changes inside Engine/ are counted apart."""
    _git(project, "add", "-A")
    stat: dict = {"files": 0, "insertions": 0, "deletions": 0, "engineFiles": 0}
    for line in _git(project, "diff", "--cached", "--numstat", "HEAD").splitlines():
        added, removed, path = line.split("\t", 2)
        if path.startswith("Engine/"):
            stat["engineFiles"] += 1
            continue
        if path.startswith(CHECKS_FOLDER):
            continue
        stat["files"] += 1
        stat["insertions"] += int(added) if added.isdigit() else 0
        stat["deletions"] += int(removed) if removed.isdigit() else 0
    return stat


# Agents -----------------------------------------------------------------------------------------------------------


def _executable(name: str) -> str:
    found = shutil.which(name)
    if not found:
        raise ToolError(f"`{name}` is not on PATH", EXIT_INFRA)
    return found


def _run_agent(command: list[str], prompt: str, project: Path, env: dict, timeout: int, log: Path,
               harness: str, prefix: str) -> tuple[int, str]:
    """Runs the agent with the prompt on stdin. Its event stream goes to `log`, and one line per command, edit or
    message is printed as it happens. On timeout the whole process tree stops."""
    with log.open("w", encoding="utf-8") as output:
        process = subprocess.Popen(command, cwd=project, env=env, stdin=subprocess.PIPE, stdout=subprocess.PIPE,
                                   stderr=subprocess.STDOUT, text=True, encoding="utf-8", errors="replace")

        def pump() -> None:
            for line in process.stdout:
                output.write(line)
                output.flush()
                described = describe(harness, line)
                if described:
                    for part in described.splitlines():
                        print(f"{prefix}   {part}", flush=True)

        reader = threading.Thread(target=pump, daemon=True)
        reader.start()
        process.stdin.write(prompt)
        process.stdin.close()
        try:
            process.wait(timeout=timeout)
            reader.join()
            return process.returncode, ""
        except subprocess.TimeoutExpired:
            if os.name == "nt":
                subprocess.run(["taskkill", "/T", "/F", "/PID", str(process.pid)], capture_output=True)
            else:
                process.kill()
            process.wait()
            reader.join(timeout=10)
            return -1, f"timed out after {timeout // 60} min"


def agent_command(harness: str, project: Path, effort: str) -> list[str]:
    """The headless agent with the pinned model and the effort (`m` or `h`)."""
    model, level = MODELS[harness], EFFORTS[effort]
    if harness == "codex":
        return [_executable("codex"), "exec", "--json", "--skip-git-repo-check", "--sandbox", "workspace-write",
                "-C", str(project), "-m", model, "-c", f'model_reasoning_effort="{level}"', "-"]
    return [_executable("claude"), "-p", "--output-format", "stream-json", "--verbose", "--permission-mode",
            "acceptEdits", "--allowedTools", CLAUDE_TOOLS, "--model", model, "--effort", level]


def sessions_of(harness: str, project: Path) -> list[Session]:
    """The attempt's sessions: the project folder is new for each attempt, so every log started there is its."""
    if harness == "codex":
        return [codex.parse(log, parent) for log, parent in codex.session_files(project, codex.home())]
    return [claude.parse(log, parent) for log, parent in claude.session_files(project, claude.home())]


def measure(sessions: list[Session]) -> dict:
    rows = [session_row(session) for session in sessions]
    main = [row for row in rows if not row["parent"]]
    tokens = {name: sum(row["tokens"][name] for row in rows) for name in rows[0]["tokens"]} if rows else {}
    categories: dict[str, int] = {}
    for session in sessions:
        for call in session.tools:
            categories[call.category] = categories.get(call.category, 0) + call.output_tokens
    return {
        "sessions": len(main), "subagents": len(rows) - len(main),
        "models": sorted({model for row in rows for model in row["models"]}),
        "efforts": sorted({effort for row in rows if not row["parent"] for effort in row["efforts"]}),
        "requests": sum(row["requests"] for row in rows),
        "toolCalls": sum(row["toolCalls"] for row in rows),
        "tokens": tokens,
        "peakContext": max((row["context"]["peak"] for row in rows), default=0),
        "startupContext": max((row["context"]["startup"] for row in main), default=0),
        "toolOutputTokens": sum(categories.values()),
        "toolOutputByCategory": dict(sorted(categories.items(), key=lambda item: -item[1])),
        "engineCommands": sum(1 for session in sessions for call in session.tools if call.engine),
        "engineFailures": sum(row["engineFailures"] for row in rows),
        "compactions": sum(row["compactions"] for row in rows),
    }


# Runs -------------------------------------------------------------------------------------------------------------


def engine_state() -> dict:
    def git(*args: str) -> str:
        done = subprocess.run(["git", "-C", str(ENGINE_ROOT), *args], capture_output=True, text=True)
        return done.stdout.strip()

    return {"commit": git("rev-parse", "--short", "HEAD"), "branch": git("rev-parse", "--abbrev-ref", "HEAD"),
            "dirty": bool(git("status", "--porcelain"))}


def attempt(task: Task, number: int, harness: str | None, effort: str, run_dir: Path, work_dir: Path,
            prebuilt: Path, files: list[str], record: dict, keep: bool, solution: bool = False) -> dict:
    """One try at a task. harness None grades the fixture as it is (with the task's solution when `solution`)."""
    name = f"{task.id}-{number}"
    project = prepare(task, work_dir / name, files, record)
    env = environment(prebuilt)
    warm(project, env, task.broken_build)
    result: dict = {"task": task.id, "attempt": number, "harness": harness, "project": str(project)}
    started = time.monotonic()
    if harness:
        log = run_dir / f"{name}.{harness}.log"
        code, problem = _run_agent(agent_command(harness, project, effort), task.prompt, project, env,
                                   task.timeout_minutes * 60, log, harness, f"[{task.id} {number}]")
        result |= {"exitCode": code, "problem": problem, "log": str(log)}
    elif solution:
        apply(project, task.folder / "solution", task.solution_edits)
    result["seconds"] = round(time.monotonic() - started, 1)
    result["checks"] = grade(task, project, env)
    result["passed"] = all(check["passed"] for check in result["checks"]) and not result.get("problem")
    if harness:
        sessions = sessions_of(harness, project)
        result["metrics"] = measure(sessions)
        result["sessionLogs"] = [str(session.path) for session in sessions]
    try:
        result["changes"] = diff_stat(project)
    except subprocess.CalledProcessError as error:  # The grade and tokens above still count
        result["changes"] = {"error": (error.stderr or str(error))[-TAIL_CHARS:]}
    if not keep and (result["passed"] or not harness):  # A failed agent attempt stays for a look
        remove(project)
        result["project"] = None
    return result


def _median(values: list[float]) -> float:
    return statistics.median(values) if values else 0


def summarize_run(attempts: list[dict]) -> dict:
    """Per task and overall: success rate and median tokens; tokens per success charges failed attempts too."""
    tasks: dict[str, dict] = {}
    for task_id in dict.fromkeys(entry["task"] for entry in attempts):
        mine = [entry for entry in attempts if entry["task"] == task_id]
        passed = [entry for entry in mine if entry["passed"]]
        metric = [entry.get("metrics") or {} for entry in mine]
        total = [m.get("tokens", {}).get("total", 0) for m in metric]
        tasks[task_id] = {
            "attempts": len(mine), "passed": len(passed), "errors": sum(1 for entry in mine if entry.get("error")),
            "medianTotal": _median(total),
            "medianTotalPassed": _median([(e.get("metrics") or {}).get("tokens", {}).get("total", 0) for e in passed]),
            "medianOutput": _median([m.get("tokens", {}).get("output", 0) for m in metric]),
            "medianRequests": _median([m.get("requests", 0) for m in metric]),
            "medianToolOutput": _median([m.get("toolOutputTokens", 0) for m in metric]),
            "medianPeakContext": _median([m.get("peakContext", 0) for m in metric]),
            "medianStartupContext": _median([m.get("startupContext", 0) for m in metric]),
            "medianEngineFailures": _median([m.get("engineFailures", 0) for m in metric]),
            "medianSeconds": _median([entry["seconds"] for entry in mine]),
            "tokensPerSuccess": round(sum(total) / len(passed)) if passed else None,
        }
    all_total = sum((e.get("metrics") or {}).get("tokens", {}).get("total", 0) for e in attempts)
    all_passed = sum(1 for e in attempts if e["passed"])
    return {"tasks": tasks,
            "overall": {"attempts": len(attempts), "passed": all_passed,
                        "errors": sum(1 for entry in attempts if entry.get("error")),
                        "successRate": round(all_passed / len(attempts), 3) if attempts else 0,
                        "totalTokens": all_total,
                        "tokensPerSuccess": round(all_total / all_passed) if all_passed else None}}


def run(tasks: list[Task], harness: str | None, repeat: int, effort: str, keep: bool,
        solution: bool = False, label: str = "run") -> tuple[Path, dict]:
    """Runs every task `repeat` times and writes RESULTS/<run id>/summary.json; returns its folder and content."""
    if not tasks:
        raise ToolError("No tasks selected; `agent-eval list` shows them (drafts need --task or --all)", EXIT_INFRA)
    run_id = datetime.now().strftime("%Y%m%d-%H%M%S") + f"-{label}"
    run_dir = RESULTS / "runs" / run_id
    run_dir.mkdir(parents=True, exist_ok=True)
    work_dir = WORK / run_id
    files = engine_files()
    record = copy_record(files)
    prebuilt = ensure_prebuilt(record_key(record), RESULTS / "prebuilt" / f"{record_key(record)}.log")
    attempts = []
    for task in tasks:
        for number in range(1, repeat + 1):
            print(f"[{task.id} {number}/{repeat}] ...", flush=True)
            try:
                entry = attempt(task, number, harness, effort, run_dir, work_dir, prebuilt, files, record, keep,
                                solution)
            except (ToolError, OSError, subprocess.SubprocessError) as error:
                entry = {"task": task.id, "attempt": number, "harness": harness, "passed": False, "seconds": 0.0,
                         "error": f"{type(error).__name__}: {error}", "project": str(work_dir / f"{task.id}-{number}")}
                print(f"[{task.id} {number}/{repeat}] the run's machinery failed, not the agent: {entry['error']}",
                      flush=True)
            attempts.append(entry)
            (run_dir / "attempts.json").write_text(json.dumps(attempts, indent=2) + "\n", encoding="utf-8")
            tokens = (entry.get("metrics") or {}).get("tokens", {}).get("total")
            print(f"[{task.id} {number}/{repeat}] {'passed' if entry['passed'] else 'FAILED'}"
                  + (f", {tokens:,} tokens" if tokens is not None else "") + f", {entry['seconds']} s"
                  + (f"; kept at {entry['project']}" if entry['project'] else ""), flush=True)
    summary = {"run": run_id, "harness": harness, "model": MODELS.get(harness or ""),
               "effort": EFFORTS[effort] if harness else None, "setting": setting(harness, effort) if harness else None,
               "repeat": repeat, "date": datetime.now().isoformat(),
               "engine": engine_state(), "engineKey": record_key(record),
               "models": sorted({m for e in attempts for m in (e.get("metrics") or {}).get("models", [])}),
               "efforts": sorted({x for e in attempts for x in (e.get("metrics") or {}).get("efforts", [])}),
               **summarize_run(attempts)}
    (run_dir / "summary.json").write_text(json.dumps(summary, indent=2) + "\n", encoding="utf-8")
    if work_dir.is_dir() and not any(work_dir.iterdir()):
        work_dir.rmdir()
    return run_dir, summary


def load_summary(name: str | None) -> dict:
    """A run's summary by id (or a unique prefix); the latest run when no name is given."""
    runs = sorted((RESULTS / "runs").glob("*/summary.json")) if (RESULTS / "runs").is_dir() else []
    if name:
        runs = [path for path in runs if path.parent.name.startswith(name)]
    else:  # The latest run of an agent, not of `verify`
        runs = [path for path in runs if json.loads(path.read_text(encoding="utf-8")).get("harness")]
    if not runs:
        raise ToolError(f"No agent-eval run{' ' + name if name else ''} under {RESULTS / 'runs'}", EXIT_INFRA)
    return json.loads(runs[-1].read_text(encoding="utf-8"))


def baseline_entry(summary: dict) -> dict:
    return {key: summary.get(key) for key in ("run", "harness", "model", "effort", "models", "repeat", "date", "engine",
                                              "tasks", "overall")}


def setting(harness: str, effort: str) -> str:
    """What a baseline is kept under: the harness and the effort, as "codex-m". The model is pinned per harness."""
    return f"{harness}-{effort}"


def read_baseline() -> dict:
    return json.loads(BASELINE.read_text(encoding="utf-8")) if BASELINE.is_file() else {}


def write_baseline(summary: dict) -> None:
    data = read_baseline()
    data[summary["setting"]] = baseline_entry(summary)
    BASELINE.write_text(json.dumps(data, indent=2) + "\n", encoding="utf-8")

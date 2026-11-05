# Agent metrics

The engine is built to be used by coding agents, so what an agent spends on a task is a property of the engine: the
docs it reads, the output of its tools, the code it needs to write. Two commands measure it in tokens.

- `agent-report` reads the session logs agents already write and summarises a project's real sessions. It costs
  nothing and shows trends, but sessions differ, so it cannot prove that one change helped.
- `agent-eval` runs fixed tasks on a fixture project with an agent and grades them by machine. The tasks and the
  starting state are the same every time, so two runs can be compared. Each run spends real tokens.

## When to measure

Run `agent-eval` before and after a change to what agents read or run: `AgentDocs/`, tool output, command names,
public API names a task touches. Report the comparison in the change. Use `--smoke` while iterating and a full run
(three attempts per task) before the change is merged; make it the baseline when it is.

## agent-report

```sh
python Engine agent-report [--harness all|claude|codex] [--since 2026-11-01] [--session ID] [--json]
```

Sessions count when they started in the project root or below (`--project DIR` measures another folder). Claude Code
logs are read from `CLAUDE_CONFIG_DIR` or `~/.claude/projects/`, subagents included; Codex logs from `CODEX_HOME` or
`~/.codex/sessions/`. Nothing leaves the machine and nothing is written.

| Section | What it says |
| --- | --- |
| Header | Sessions, subagent sessions, prompts typed, model requests, tool calls, compactions, models |
| Tokens | All tokens the models read and wrote: cache read, cache write, uncached input, output (reasoning included). Per session medians, the context of the first request (system prompt, tools, always-loaded agent files), and the context per request with how many requests went past 100K, 200K and 400K |
| Tool output | Tokens of tool results by kind: reads, searches, engine commands, edits, git, other shell, web, subagents. Estimated as characters / 4, which is what an agent first reads; every later request reads them again from its context |
| Engine commands | Each `python Engine ...` command: runs, failures, output tokens; the longest run of failed engine commands in a row (a build-fix loop) |
| File reads | Files read by tools or by `cat`/`sed`/`Get-Content`, by area (game, engine source, engine Docs, AgentDocs, agent files), the largest, and those read three times or more |
| AgentDocs | Which AgentDocs files were read and which skills were never read |
| Behaviour | Habits that cost tokens, counted from the tool calls: engine source files read (`Engine/Core`, `Editor`, `Assets`, `Tools`, `ThirdParty`) instead of AgentDocs; searches over the whole project or the whole engine copy, with the tokens they returned; `api` queries, which should replace those searches; editor commands that failed (usually no editor open); sessions whose first edit came before any skill was read |
| Largest tool outputs | The single biggest results, with the command or file |
| Sessions by tokens | The heaviest sessions with requests, peak context, total and output tokens, failed engine commands |

Where the cost goes: every request reads the whole context again, so total tokens grow with context size times
requests. Cache reads are cheaper per token than new input or output, but they are most of the tokens.

Claude Code writes one log line per part of a response and repeats the usage on each; requests are counted once per
response. Codex reports cached input inside input; the report splits it out. A Codex script that runs several commands counts as one tool call per command when its output reports one exit code per command; otherwise its whole output stays with the script. Either way, engine commands and file reads count one by one: a `;` chain, a command template over a list (`queries.map(q => ... python Engine api "${q}")`) and `$p='X.h'; Get-Content $p` are all seen, and a call that ran several shares its output evenly between them.

## agent-eval

```sh
python Engine agent-eval list
python Engine agent-eval verify [--task ID] [--all]
python Engine agent-eval run [--harness codex|claude] [--effort m|h] [--repeat 3] [--smoke] [--task ID] [--all]
python Engine agent-eval watch [RUN]
python Engine agent-eval show [RUN]
python Engine agent-eval compare [RUN] [--against RUN]
python Engine agent-eval baseline [RUN]
```

### Model and effort

The model is pinned per harness, so a change to someone's own agent settings cannot move the numbers: Codex runs
`gpt-5.6-sol`, Claude Code runs `claude-opus-5-5`. `--effort m` (medium, the default) or `--effort h` (high) sets the
reasoning effort. A run records the model and effort, and `show` warns when the session logs name another model or
effort than the pinned ones. Baselines are kept per harness and effort (`codex-m`, `codex-h`, `claude-m`, `claude-h`),
and `compare` uses the one of the run's setting.

### What an attempt is

1. A fresh project in the system temp folder (`engine-agent-eval/<run>/<task>-<n>`), away from any folder with agent
   files: the template files `new` adds, the fixture (`Tools/AgentEval/Fixture`), and the engine as a released copy
   would hold it — this working tree with uncommitted changes, without ignored, export-ignored and `Tools/AgentEval`
   files — with its `EngineCopy.json`. The project is its own git repository. On Windows the folder first grants the
   current user full access to everything created in it: Codex's sandbox writes new files as its own user, and
   without the grant they could not be graded or deleted.
2. The engine package is built once per engine state (`build/agent-eval/prebuilt/<key>`, keyed by the files that reach
   the compiler) and linked through `ENGINE_PREBUILT`, so the agent's builds take seconds.
3. The game and the simulation tool are built before the agent starts; that time is not the agent's.
4. The agent runs headless with the task's prompt and the timeout of the task:
   - Codex: `codex exec --json --sandbox workspace-write -m gpt-5.6-sol` with the effort (writes only inside the
     project, no network).
   - Claude Code: `claude -p --model claude-opus-5-5 --effort ...` with edits accepted and only `Read`, `Edit`, `Write`, `Grep`, `Glob` and
     `Bash(python Engine:*)` allowed.
   Variables of an agent or editor session that started the run are removed from its environment.
   While it runs, `run` prints one line per command (with its exit code), edit and message of the agent; `watch`
   prints the same for a run started in another terminal. The full event stream is in
   `build/agent-eval/runs/<run>/<task>-<n>.<harness>.log`.
5. The task's hidden checks are copied into `.eval-checks/` and run; the attempt passes when all pass.
6. The attempt's own session logs (the project folder is new, so every log started there is the attempt's) give its
   tokens; `git` gives what it changed. A passing project is deleted; a failing one stays for a look.

### Reading a run

`show` lists, per task, attempts passed and medians, and the behaviour counts over all attempts: total tokens, output, requests, tool output, peak context,
failed engine commands, seconds, and tokens per success (all tokens of the task divided by passed attempts, so failed
attempts are charged). `compare` puts a run next to the committed baseline of its setting, or next to another run.
Token counts compare only within one harness, model and effort: tokenizers, harnesses and effort levels differ. Judge a change by tokens per
success and pass rate together; fewer tokens with fewer passes is not a gain.

`baseline` writes a run's summary into `Tools/AgentEval/Baseline.json` under its setting (`codex-m`); commit it.

### Cost

An attempt is a real agent session: tens of requests on a small project. A full run is tasks × 3 attempts; a smoke run
is the smoke tasks once. Run the full set when a change is ready, not on every commit.

### Tasks

A task is a folder in `Tools/AgentEval/Tasks/<id>/`:

| File | Use |
| --- | --- |
| `task.json` | `title`, `kind`, `status` (`draft` until reviewed, then `ready`; `run` takes ready tasks unless `--task` or `--all`), `smoke`, `timeoutMinutes`, `prompt`, `notes`, `brokenBuild` (the setup breaks the build on purpose), `setupEdits`, `solutionEdits`, `checks` |
| `setup/` | Files copied over the project before the agent starts |
| `solution/` | Files of the reference solution, with `solutionEdits` |
| `checks/` | Hidden check files: scenarios and scripts, copied in only to grade |

Edits are `{"file", "old", "new"}`; `old` must occur exactly once. Check types:

| Type | Passes when |
| --- | --- |
| `build` | `python Engine build` succeeds (warnings fail it) |
| `check` | `python Engine check` finds no include-boundary or tuning problem (the engine's own AgentDocs are not graded) |
| `sim` | `python Engine sim run .eval-checks/<scenario>` succeeds |
| `scenarios` | At least `min` (default 1) scenarios matching `glob`, outside `exclude` and holding every `contains` text, exist and all pass |
| `grep` | `pattern` occurs at least `min` times (default 1) in files matching `glob`, or at most `max` times |
| `python` | `.eval-checks/<script> PROJECT` exits 0 |

Prefer behaviour to text: a scenario that fails on the fixture and passes on a correct change. Text checks are for
looks that no headless check can see; say so in `notes`. `verify` runs every selected task twice without an agent:
its checks must fail on the fixture and pass with the reference solution. Run it after changing the fixture or a task.

Prompts are written as a developer would ask, without file names the developer would not know. Tasks represent
work on any game, not one game.

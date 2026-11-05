"""agent-report and agent-eval without an agent: log parsing, classification, summaries and the task files."""

import json
import tempfile
import unittest
from pathlib import Path

from Tools.agentmetrics import claude, codex, evaluation
from Tools.agentmetrics.live import describe
from Tools.agentmetrics.classify import area, engine_command, read_files, shell_category
from Tools.agentmetrics.report import render, summarize
from Tools.commands.agent_eval import render_compare, render_summary
from Tools.commands.agent_report import select
from Tools.errors import ToolError
from Tools.manifest import ENGINE_ROOT

CHECK_TYPES = {"build", "check", "sim", "scenarios", "grep", "python"}


def write_lines(path: Path, entries: list[dict]) -> Path:
    path.parent.mkdir(parents=True, exist_ok=True)
    path.write_text("".join(json.dumps(entry, separators=(",", ":")) + "\n" for entry in entries), encoding="utf-8")
    return path


def claude_log(root: Path) -> list[dict]:
    usage = {"input_tokens": 3, "cache_read_input_tokens": 1000, "cache_creation_input_tokens": 200,
             "output_tokens": 50}
    def assistant(block: dict, output: int = 50) -> dict:
        return {"type": "assistant", "cwd": str(root), "timestamp": "2026-11-05T10:00:00Z",
                "message": {"id": "msg_1", "model": "claude-test", "usage": usage | {"output_tokens": output},
                            "content": [block]}}
    return [
        {"type": "user", "cwd": str(root), "timestamp": "2026-11-05T09:59:00Z",
         "message": {"role": "user", "content": "Add a pause menu"}},
        assistant({"type": "thinking", "thinking": ""}, 10),  # The same response written as two lines
        assistant({"type": "tool_use", "id": "t1", "name": "Bash", "input": {"command": "python Engine build"}}),
        {"type": "user", "cwd": str(root), "message": {"role": "user", "content": [
            {"type": "tool_result", "tool_use_id": "t1", "is_error": True, "content": "x" * 400}]}},
        {"type": "assistant", "cwd": str(root), "message": {
            "id": "msg_2", "model": "claude-test", "usage": usage,
            "content": [{"type": "tool_use", "id": "t2", "name": "Read",
                         "input": {"file_path": str(root / "Game" / "Player.cpp")}}]}},
        {"type": "user", "cwd": str(root), "message": {"role": "user", "content": [
            {"type": "tool_result", "tool_use_id": "t2", "content": [{"type": "text", "text": "y" * 800}]}]}},
        {"type": "system", "subtype": "compact_boundary", "cwd": str(root)},
        {"type": "ai-title", "aiTitle": "Pause menu"},
    ]


def codex_log(root: Path) -> list[dict]:
    record = {"input_tokens": 1000, "cached_input_tokens": 800, "cache_write_input_tokens": 0, "output_tokens": 40,
              "reasoning_output_tokens": 10}
    script = 'text(await tools.exec_command({cmd:"python Engine test --label x","max_output_tokens":6000}));'
    return [
        {"type": "session_meta", "payload": {"id": "thread-1", "cwd": str(root)}},
        {"type": "turn_context", "payload": {"model": "codex-test", "effort": "medium"}},
        {"type": "event_msg", "timestamp": "2026-11-05T11:00:00Z",
         "payload": {"type": "user_message", "message": "Fix the build"}},
        {"type": "token_usage_record", "payload": {"response_id": "r1", "usage": record}},
        {"type": "token_usage_record", "payload": {"response_id": "r1", "usage": record}},
        {"type": "response_item", "payload": {"type": "custom_tool_call", "name": "exec", "call_id": "c1",
                                              "input": script}},
        {"type": "response_item", "payload": {"type": "custom_tool_call_output", "call_id": "c1", "output": [
            {"type": "input_text", "text": "Script completed"},
            {"type": "input_text", "text": json.dumps({"exit_code": 1, "output": "z" * 200})}]}},
        {"type": "compacted", "payload": {}},
    ]


class ClassifyTests(unittest.TestCase):
    def test_engine_commands(self):
        self.assertEqual(engine_command("python Engine build"), "build")
        self.assertEqual(engine_command('cd x && python . ctl open a --screenshot'), "ctl open")
        self.assertEqual(engine_command("python ../engine --project EditorTest ctl --json status"), "ctl status")
        self.assertEqual(engine_command('python "C:/a b/engine" sim run a.json'), "sim run")
        self.assertIsNone(engine_command("cd ../engine-atmosphere && sed -n 1,5p x.h"))
        self.assertIsNone(engine_command("python - <<EOF\nengine sim in\nEOF"))
        self.assertIsNone(engine_command("python -m unittest discover"))

    def test_reads_and_categories(self):
        self.assertEqual(read_files("cat Game/A.cpp; sed -n 1,40p 'Game/B c.h'"), ["Game/A.cpp", "Game/B c.h"])
        self.assertEqual(read_files("Get-Content -LiteralPath Engine\\Core\\X.h -TotalCount 40"),
                         ["Engine\\Core\\X.h"])
        self.assertEqual(shell_category("cd x && cat a.txt"), "read")
        self.assertEqual(shell_category("rg -n Foo Game"), "search")
        self.assertEqual(shell_category("git diff --stat"), "git")
        self.assertEqual(shell_category("python Engine test"), "engine")
        self.assertEqual(area("Engine/AgentDocs/Skills/ui/SKILL.md"), "AgentDocs")
        self.assertEqual(area("Game/Scenes/Play.cpp"), "game")
        self.assertEqual(area("Engine/Core/Ui/Ui.h"), "engine source")
        self.assertEqual(area("Engine/Docs/Ui.md"), "engine Docs")


class LiveTests(unittest.TestCase):
    def test_codex_events(self):
        command = {"type": "item.completed", "item": {
            "type": "command_execution", "exit_code": 1,
            "command": r'''"C:\Windows\System32\WindowsPowerShell\v1.0\powershell.exe" -Command 'python Engine build' '''}}
        self.assertEqual(describe("codex", json.dumps(command)), "$ python Engine build  -> exit 1")
        edit = {"type": "item.completed", "item": {"type": "file_change", "changes": [
            {"path": r"C:\Temp\engine-agent-eval\run-1\dash-1\Game\A.cpp"}]}}
        self.assertEqual(describe("codex", json.dumps(edit)), "edit Game/A.cpp")
        self.assertIsNone(describe("codex", "not json"))

    def test_claude_events(self):
        event = {"type": "assistant", "message": {"content": [
            {"type": "tool_use", "name": "Bash", "input": {"command": "python Engine test"}},
            {"type": "text", "text": "Done."}]}}
        self.assertEqual(describe("claude", json.dumps(event)), "Bash: python Engine test\nsays: Done.")


class CodexScriptTests(unittest.TestCase):
    def test_command_shapes(self):
        keyed = 'const r = await tools.shell_command({command:"Get-Content -Raw Engine/AgentDocs/Agent.md",' \
                '"workdir":"C:\\\\x","timeout_ms":10000}); text(r)'
        call = codex._call("exec", keyed)
        self.assertEqual((call.category, call.files), ("read", ["Engine/AgentDocs/Agent.md"]))
        listed = 'const calls = [["status", "python Engine ctl status"], ["git", "git status --short"]];\n' \
                 'await Promise.all(calls.map(([k, c]) => tools.shell_command({command: c})));'
        call = codex._call("exec", listed)
        self.assertEqual((call.category, call.engine), ("engine", "ctl status"))
        self.assertEqual(call.target, "python Engine ctl status ; git status --short")  # Labels are not commands


class ParseTests(unittest.TestCase):
    def setUp(self):
        self.temp = tempfile.TemporaryDirectory()
        self.root = Path(self.temp.name) / "My Game"
        self.root.mkdir()

    def tearDown(self):
        self.temp.cleanup()

    def test_claude_session(self):
        home = Path(self.temp.name) / "claude"
        folder = home / "projects" / claude.encode(self.root)
        write_lines(folder / "s1.jsonl", claude_log(self.root))
        write_lines(folder / "s1" / "subagents" / "agent-a.jsonl", claude_log(self.root))
        write_lines(home / "projects" / "Other" / "s2.jsonl", claude_log(Path("C:/Other")))
        found = claude.session_files(self.root, home)
        self.assertEqual(sorted(parent or "" for _, parent in found), ["", "s1"])
        session = claude.parse(folder / "s1.jsonl")
        self.assertEqual(len(session.requests), 2)  # msg_1 counted once
        self.assertEqual(session.requests[0].output, 50)
        self.assertEqual(session.requests[0].context, 1203)
        self.assertEqual(session.title, "Pause menu")
        self.assertEqual((session.prompts, session.compactions), (1, 1))
        build, read = session.tools
        self.assertEqual((build.engine, build.failed, build.output_tokens), ("build", True, 100))
        self.assertEqual((read.category, read.output_tokens), ("read", 200))

    def test_codex_session(self):
        home = Path(self.temp.name) / "codex"
        log = write_lines(home / "sessions" / "2026" / "11" / "05" / "rollout-1.jsonl", codex_log(self.root))
        write_lines(home / "sessions" / "2026" / "11" / "05" / "rollout-2.jsonl", codex_log(Path("C:/Other")))
        self.assertEqual([path for path, _ in codex.session_files(self.root, home)], [log])
        session = codex.parse(log)
        self.assertEqual(len(session.requests), 1)
        request = session.requests[0]
        self.assertEqual((request.input, request.cache_read, request.output, request.reasoning), (200, 800, 40, 10))
        self.assertEqual((session.prompts, session.compactions, session.title), (1, 1, "Fix the build"))
        call = session.tools[0]
        self.assertEqual((call.category, call.engine, call.failed), ("engine", "test", True))
        self.assertEqual((session.models, session.efforts), (["codex-test"], ["medium"]))

    def test_summary_and_selection(self):
        home = Path(self.temp.name) / "claude"
        folder = home / "projects" / claude.encode(self.root)
        write_lines(folder / "s1.jsonl", claude_log(self.root))
        sessions = [claude.parse(folder / "s1.jsonl")]
        summary = summarize(sessions, self.root, ENGINE_ROOT, 5)
        self.assertEqual(summary["counts"]["requests"], 2)
        self.assertEqual(summary["engineCommands"]["build"]["failed"], 1)
        self.assertIn("Game/Player.cpp", summary["reads"]["top"])
        self.assertIn("ui", summary["agentDocs"]["skillsNeverRead"])
        self.assertIn("Engine commands", render(summary))
        from datetime import date
        self.assertEqual(select(sessions, since=date(2026, 11, 6)), [])
        self.assertEqual(len(select(sessions, since=date(2026, 11, 5))), 1)


class EvaluationTests(unittest.TestCase):
    def test_tasks_are_complete(self):
        tasks = evaluation.load_tasks()
        self.assertGreaterEqual(len(tasks), 5)
        for task in tasks:
            with self.subTest(task=task.id):
                self.assertTrue(task.prompt and task.checks)
                self.assertIn(task.status, {"draft", "ready"})
                for check in task.checks:
                    self.assertIn(check["type"], CHECK_TYPES)
                    for key in ("scenario", "script"):
                        if key in check:
                            self.assertTrue((task.folder / "checks" / check[key]).is_file(), check[key])
                for edit in task.setup_edits + task.solution_edits:
                    self.assertEqual(set(edit), {"file", "old", "new"})
                self.assertTrue(task.solution_edits or (task.folder / "solution").is_dir(), "no reference solution")

    def test_pinned_agent_commands(self):
        project = Path("C:/project")
        codex_command = evaluation.agent_command("codex", project, "m")
        self.assertIn("gpt-5.6-sol", codex_command)
        self.assertIn('model_reasoning_effort="medium"', codex_command)
        claude_command = evaluation.agent_command("claude", project, "h")
        self.assertEqual(claude_command[claude_command.index("--model") + 1], "claude-opus-5-5")
        self.assertEqual(claude_command[claude_command.index("--effort") + 1], "high")
        self.assertEqual(evaluation.setting("codex", "m"), "codex-m")

    def test_unknown_task(self):
        with self.assertRaises(ToolError):
            evaluation.load_tasks(["no-such-task"])

    def test_edits(self):
        with tempfile.TemporaryDirectory() as temp:
            project = Path(temp)
            (project / "a.txt").write_text("one two\n", encoding="utf-8")
            evaluation.apply(project, project / "none", [{"file": "a.txt", "old": "two", "new": "three"}])
            self.assertEqual((project / "a.txt").read_text(encoding="utf-8"), "one three\n")
            with self.assertRaises(ToolError):
                evaluation.apply(project, project / "none", [{"file": "a.txt", "old": "two", "new": "x"}])

    def test_remove_takes_read_only_files(self):
        import os
        import stat
        with tempfile.TemporaryDirectory() as temp:
            folder = Path(temp) / "attempt"
            (folder / ".git" / "objects").mkdir(parents=True)
            locked = folder / ".git" / "objects" / "ab"
            locked.write_text("x", encoding="utf-8")
            os.chmod(locked, stat.S_IREAD)
            evaluation.remove(folder)
            self.assertFalse(folder.exists())

    def test_engine_copy(self):
        files = evaluation.engine_files()
        self.assertIn("Core/CMakeLists.txt", files)
        self.assertFalse(any(name.startswith("Tools/AgentEval/") for name in files))
        self.assertNotIn("AGENTS.md", files)
        self.assertFalse(any(name.startswith("build/") for name in files))
        record = {"files": {"Core/A.cpp": "sha256:1", "Docs/A.md": "sha256:2"}}
        changed_doc = {"files": {"Core/A.cpp": "sha256:1", "Docs/A.md": "sha256:3"}}
        changed_code = {"files": {"Core/A.cpp": "sha256:4", "Docs/A.md": "sha256:2"}}
        self.assertEqual(evaluation.record_key(record), evaluation.record_key(changed_doc))
        self.assertNotEqual(evaluation.record_key(record), evaluation.record_key(changed_code))

    def test_run_summary(self):
        def entry(task, passed, total):
            return {"task": task, "passed": passed, "seconds": 10.0,
                    "metrics": {"tokens": {"total": total, "output": 10}, "requests": 4}}
        summary = evaluation.summarize_run([entry("a", True, 100), entry("a", False, 300), entry("b", True, 50)])
        self.assertEqual(summary["tasks"]["a"]["tokensPerSuccess"], 400)
        self.assertEqual(summary["tasks"]["a"]["medianTotal"], 200)
        self.assertEqual(summary["overall"]["tokensPerSuccess"], 225)
        full = {"run": "r", "harness": "codex", "model": "gpt-5.6-sol", "effort": "medium",
                "models": ["gpt-5.6-sol", "other-model"], "efforts": ["medium", "xhigh"],
                "engine": {"commit": "abc", "dirty": False}, **summary}
        text = render_summary(full)
        self.assertIn("tokens per success", text)
        self.assertIn("warning: the logs also name other-model", text)
        self.assertIn("warning: the logs show effort xhigh", text)
        self.assertIn("overall", render_compare(full, full, "itself"))


if __name__ == "__main__":
    unittest.main()

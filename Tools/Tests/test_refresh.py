"""`refresh` without an editor: finding the endpoint, reading replies, result → exit code; no compiler."""

import io
import json
import tempfile
import unittest
from pathlib import Path

from Tools.commands.refresh import ANNOUNCEMENT, VERSION, endpoint, exit_code, replies
from Tools.errors import EXIT_FAILED, EXIT_INFRA, EXIT_OK, ToolError


class EndpointTests(unittest.TestCase):
    def setUp(self):
        self._directory = tempfile.TemporaryDirectory(prefix="EngineRefresh-")
        self.build = Path(self._directory.name)

    def tearDown(self):
        self._directory.cleanup()

    def announce(self, data: dict):
        (self.build / ANNOUNCEMENT).write_text(json.dumps(data), encoding="utf-8")

    def test_no_open_editor_is_infrastructure(self):
        with self.assertRaises(ToolError) as raised:
            endpoint(self.build)
        self.assertEqual(raised.exception.exit_code, EXIT_INFRA)
        self.assertIn("python Engine editor", str(raised.exception))

    def test_announced_pipe(self):
        self.announce({"version": VERSION, "pipe": r"\\.\pipe\EngineEditor.Control.x", "pid": 1})
        self.assertEqual(endpoint(self.build), r"\\.\pipe\EngineEditor.Control.x")

    def test_other_protocol_version_is_refused(self):
        self.announce({"version": VERSION + 1, "pipe": "p", "pid": 1})
        with self.assertRaises(ToolError):
            endpoint(self.build)


class ReplyTests(unittest.TestCase):
    def test_lines_split_across_reads(self):
        class Chunks(io.RawIOBase):
            def __init__(self, data: bytes):
                self.data = data

            def read(self, size=-1):
                chunk, self.data = self.data[:3], self.data[3:]
                return chunk

        stream = Chunks(b'{"type": "log", "text": "a"}\n\n{"type": "result", "result": "succeeded"}\n')
        self.assertEqual([reply["type"] for reply in replies(stream)], ["log", "result"])

    def test_only_success_exits_zero(self):
        self.assertEqual(exit_code("succeeded"), EXIT_OK)
        for result in ("build-failed", "refresh-failed", "canceled"):
            self.assertEqual(exit_code(result), EXIT_FAILED, result)


if __name__ == "__main__":
    unittest.main()

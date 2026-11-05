"""`refresh` without an editor: finding the endpoint, reading replies, result → exit code; no compiler."""

import ctypes
import errno
import io
import json
import os
import tempfile
import threading
import unittest
from ctypes import wintypes
from pathlib import Path
from unittest.mock import patch

from Tools.commands.refresh import ANNOUNCEMENT, VERSION, connect, endpoint, exit_code, replies
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


class ConnectTests(unittest.TestCase):
    """A real pipe with one instance, like the editor's: what open() raises for it is what connect has to recognise."""

    def test_waits_while_another_client_holds_the_endpoint(self):
        kernel = ctypes.WinDLL("kernel32", use_last_error=True)
        kernel.CreateNamedPipeW.restype = wintypes.HANDLE
        kernel.CreateNamedPipeW.argtypes = [wintypes.LPCWSTR, *[wintypes.DWORD] * 6, wintypes.LPVOID]
        kernel.DisconnectNamedPipe.argtypes = [wintypes.HANDLE]
        kernel.ConnectNamedPipe.argtypes = [wintypes.HANDLE, wintypes.LPVOID]
        kernel.CloseHandle.argtypes = [wintypes.HANDLE]
        pipe = f"\\\\.\\pipe\\EngineToolsTest.{os.getpid()}"
        access_duplex, no_wait, one_instance, buffer_bytes = 3, 1, 1, 4096
        server = kernel.CreateNamedPipeW(pipe, access_duplex, no_wait, one_instance, buffer_bytes, buffer_bytes, 0,
                                         None)
        self.addCleanup(kernel.CloseHandle, server)
        holder = open(pipe, "r+b", buffering=0)
        self.addCleanup(holder.close)

        def free_the_endpoint():
            holder.close()
            kernel.DisconnectNamedPipe(server)
            kernel.ConnectNamedPipe(server, None)  # The instance takes a client again only after this

        timer = threading.Timer(0.2, free_the_endpoint)
        timer.start()
        self.addCleanup(timer.join)
        with connect(pipe) as stream:
            self.assertTrue(stream.writable())

    def test_other_errors_do_not_wait(self):
        with patch("builtins.open", side_effect=PermissionError(errno.EACCES, "denied")), \
                self.assertRaisesRegex(ToolError, "Cannot reach the editor"):
            connect("pipe")


if __name__ == "__main__":
    unittest.main()

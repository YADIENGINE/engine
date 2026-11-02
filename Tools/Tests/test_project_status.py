"""The status files a launcher reads (Docs/ToolsContract.md, Project status files)."""

import json
import tempfile
import unittest
from datetime import datetime, timezone
from pathlib import Path

from Tools.project_status import LAST_BUILD_FILE, write_last_build


class LastBuildTests(unittest.TestCase):
    def test_records_the_result_and_a_utc_time(self):
        with tempfile.TemporaryDirectory() as folder:
            build_dir = Path(folder) / "new" / "build"
            write_last_build(build_dir, False, datetime(2026, 11, 2, 12, 30, 5, tzinfo=timezone.utc))
            written = json.loads((build_dir / LAST_BUILD_FILE).read_text(encoding="utf-8"))
        self.assertEqual(written, {"ok": False, "finished": "2026-11-02T12:30:05Z"})

    def test_an_unwritable_place_does_not_fail_the_build(self):
        with tempfile.TemporaryDirectory() as folder:
            blocked = Path(folder) / "file"
            blocked.write_text("x", encoding="utf-8")
            write_last_build(blocked, True)  # A file where the folder should be
            self.assertEqual(blocked.read_text(encoding="utf-8"), "x")


if __name__ == "__main__":
    unittest.main()

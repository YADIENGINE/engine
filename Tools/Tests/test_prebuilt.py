"""Which compiled engine a build links (Docs/Prebuilt.md); no compiler."""

import hashlib
import json
import os
import tempfile
import unittest
from pathlib import Path
from unittest.mock import patch

from Tools.prebuilt import PREBUILT_VARIABLE, cache_definition, modified_files, prebuilt_directory
from Tools.project import Project


class PrebuiltTests(unittest.TestCase):
    def setUp(self):
        self.temp = tempfile.TemporaryDirectory(ignore_cleanup_errors=True)
        self.root = Path(self.temp.name)
        self.addCleanup(self.temp.cleanup)
        self.engine = self.root / "Project" / "Engine"
        self.engine.mkdir(parents=True)
        (self.engine / "Game.cpp").write_text("code", newline="\n")
        self.package = self.root / "Package"
        self.package.mkdir()
        (self.package / "Prebuilt.json").write_text("{}")
        self.write_project({})
        self.write_record()
        environment = patch.dict(os.environ)
        environment.start()
        self.addCleanup(environment.stop)
        os.environ.pop(PREBUILT_VARIABLE, None)

    def write_project(self, extra: dict) -> None:
        (self.root / "Project" / "Project.json").write_text(json.dumps({"version": 1, "name": "MyGame", **extra}))

    def write_record(self) -> None:
        digest = hashlib.sha256((self.engine / "Game.cpp").read_bytes()).hexdigest()
        (self.engine / "EngineCopy.json").write_text(json.dumps({"files": {"Game.cpp": f"sha256:{digest}"}}))

    def directory(self) -> Path | None:
        return prebuilt_directory(Project(self.root / "Project"), self.engine)[0]

    def test_no_package_builds_from_source(self):
        self.assertIsNone(self.directory())

    def test_the_launchers_package_is_used(self):
        os.environ[PREBUILT_VARIABLE] = str(self.package)
        self.assertEqual(self.directory(), self.package)

    def test_commands_without_the_launcher_keep_its_choice(self):
        os.environ[PREBUILT_VARIABLE] = str(self.package)
        self.directory()
        del os.environ[PREBUILT_VARIABLE]
        self.assertEqual(self.directory(), self.package)

    def test_the_project_can_choose_source(self):
        os.environ[PREBUILT_VARIABLE] = str(self.package)
        self.write_project({"engine": {"build": "source"}})
        self.assertIsNone(self.directory())

    def test_a_checkout_without_a_record_builds_from_source(self):
        os.environ[PREBUILT_VARIABLE] = str(self.package)
        (self.engine / "EngineCopy.json").unlink()
        self.assertIsNone(self.directory())

    def test_an_edited_engine_builds_from_source(self):
        os.environ[PREBUILT_VARIABLE] = str(self.package)
        self.assertEqual(self.directory(), self.package)
        edited = self.engine / "Game.cpp"
        edited.write_text("changed")
        future = edited.stat().st_mtime + 10
        os.utime(edited, (future, future))
        self.assertIsNone(self.directory())

    def test_modified_files_finds_changed_missing_and_added(self):
        (self.engine / "Added.h").write_text("x")
        record = {"files": {"Game.cpp": "sha256:other", "Missing.h": "sha256:x"}}
        self.assertEqual(sorted(modified_files(self.engine, record)), ["Added.h", "Game.cpp", "Missing.h"])

    def test_the_cache_value_clears_when_no_package_applies(self):
        self.assertEqual(cache_definition(None), "-DENGINE_PREBUILT_DIR=")


if __name__ == "__main__":
    unittest.main()

"""Build error summaries from compiler, linker and CMake output; no compiler."""

import unittest
from pathlib import Path

from Tools.build_errors import MAX_SHOWN, build_errors, summary

ROOT = Path("C:/Work/MyGame")


class BuildErrorTests(unittest.TestCase):
    def test_msvc_error_with_column_relative_to_the_project(self):
        errors = build_errors([r"C:\Work\MyGame\Game\Player.cpp(42,17): error C2065: 'scoer': undeclared identifier"],
                              ROOT)
        self.assertEqual(str(errors[0]), "Game/Player.cpp:42:17: C2065 'scoer': undeclared identifier")

    def test_msvc_fatal_error_without_column(self):
        error = build_errors(["Game/Hud.h(3): fatal error C1083: Cannot open include file"])[0]
        self.assertEqual((error.file, error.line, error.column, error.code), ("Game/Hud.h", 3, 0, "C1083"))

    def test_linker_gnu_cmake_and_tool_errors(self):
        lines = [
            "MyGame.obj : error LNK2019: unresolved external symbol Draw",
            "src/a.cpp:7:3: error: expected ';'",
            "CMake Error at Game/CMakeLists.txt:12 (add_executable):",
            "  Cannot find source file: Missing.cpp",
            'Error copying directory from "Game/Assets" to "build/Assets".',
        ]
        self.assertEqual([str(error) for error in build_errors(lines)], [
            "MyGame.obj: LNK2019 unresolved external symbol Draw",
            "src/a.cpp:7:3: expected ';'",
            "Game/CMakeLists.txt:12: CMake Cannot find source file: Missing.cpp",
            'Error copying directory from "Game/Assets" to "build/Assets".',
        ])

    def test_warnings_progress_and_repeats_are_ignored(self):
        line = "Game/Player.cpp(42): error C2065: 'x': undeclared identifier"
        lines = ["[3/40] Building CXX object", "Game/Player.cpp(9): warning C4100: unused", "FAILED: x.obj", line, line]
        self.assertEqual(len(build_errors(lines)), 1)

    def test_summary(self):
        self.assertEqual(summary(["all good"]), [])
        lines = [f"Game/F.cpp({n}): error C2065: x" for n in range(1, MAX_SHOWN + 3)]
        printed = summary(lines)
        self.assertEqual(printed[0], f"Errors ({MAX_SHOWN + 2}):")
        self.assertEqual(printed[-1], "  ... and 2 more")


if __name__ == "__main__":
    unittest.main()

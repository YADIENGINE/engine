"""Internals sınırı denetiminin kuralları; derleyici gerekmez."""

import tempfile
import unittest
from pathlib import Path

from Tools.include_boundaries import violations

HEADERS = ["Core/Input/Internals/Reader.h", "Core/Modules/Saves/Internals/State.h", "Editor/Internals/Session.h",
           "Terminal/Internals/Screen.h", "Core/Input/Input.h"]


class BoundaryTests(unittest.TestCase):
    def setUp(self):
        self._directory = tempfile.TemporaryDirectory(prefix="EngineBoundaries-")
        self.root = Path(self._directory.name)
        self.engine = self.root / "Engine"
        for header in HEADERS:
            self.write(f"Engine/{header}", "#pragma once\n")

    def tearDown(self):
        self._directory.cleanup()

    def write(self, relative: str, text: str):
        path = self.root / relative
        path.parent.mkdir(parents=True, exist_ok=True)
        path.write_text(text, encoding="utf-8")

    def check(self):
        return violations(self.root, self.engine)

    def test_game_may_use_public_headers_and_its_own_internals(self):
        self.write("Game/Tracking/Internals/State.h", "#pragma once\n")
        self.write("Game/Scene.cpp", '#include "Input/Input.h"\n#include "Tracking/Internals/State.h"\n')
        self.assertEqual(self.check(), [])

    def test_game_may_not_use_engine_internals(self):
        self.write("Game/Scene.cpp", '#include "Input/Input.h"\n#include "Input/Internals/Reader.h"\n')
        self.write("tests/SaveTests.cpp", '  #  include "Modules/Saves/Internals/State.h"\n')
        found = self.check()
        self.assertEqual(len(found), 2)
        self.assertTrue(found[0].startswith("Game/Scene.cpp:2:"))
        self.assertIn("private to Modules/Saves", found[1])

    def test_core_internals_are_shared_inside_engine(self):
        self.write("Engine/Core/Runtime/Game.cpp", '#include "Input/Internals/Reader.h"\n')
        self.write("Engine/Editor/Host/Host.cpp", '#include "Input/Internals/Reader.h"\n')
        self.write("Engine/Core/Modules/Saves/Saves.cpp", '#include "Input/Internals/Reader.h"\n')
        self.assertEqual(self.check(), [])

    def test_module_internals_stay_in_their_module(self):
        self.write("Engine/Core/Modules/Saves/Tests/StateTests.cpp", '#include "Modules/Saves/Internals/State.h"\n')
        self.write("Engine/Core/Modules/Other/Other.cpp", '#include "Modules/Saves/Internals/State.h"\n')
        self.write("Engine/Core/Runtime/Game.cpp", '#include "Modules/Saves/Internals/State.h"\n')
        found = self.check()
        self.assertEqual([entry.split(":")[0] for entry in found],
                         ["Engine/Core/Modules/Other/Other.cpp", "Engine/Core/Runtime/Game.cpp"])

    def test_editor_internals_stay_in_editor(self):
        self.write("Engine/Editor/Ui/Panel.cpp", '#include "Editor/Internals/Session.h"\n')
        self.write("Engine/Core/Runtime/Game.cpp", '#include "Editor/Internals/Session.h"\n')
        found = self.check()
        self.assertEqual(len(found), 1)
        self.assertIn("private to Editor", found[0])

    def test_terminal_internals_stay_in_terminal(self):
        self.write("Engine/Terminal/Tests/ScreenTests.cpp", '#include "Terminal/Internals/Screen.h"\n')
        self.write("Engine/Editor/Ui/Console.cpp", '#include "Terminal/Internals/Screen.h"\n')
        found = self.check()
        self.assertEqual(len(found), 1)
        self.assertTrue(found[0].startswith("Engine/Editor/Ui/Console.cpp:1:"))
        self.assertIn("private to Terminal", found[0])

    def test_build_trees_are_not_scanned(self):
        self.write("build/CMakeCache.txt", "")
        self.write("build/_deps/Generated.cpp", '#include "Input/Internals/Reader.h"\n')
        self.write(".git/Hook.cpp", '#include "Input/Internals/Reader.h"\n')
        self.assertEqual(self.check(), [])

    def test_other_engine_copies_in_the_project_are_not_scanned(self):
        self.write("Engine/EngineManifest.json", "{}")
        self.write("Engine/Core/Runtime/Game.cpp", '#include "Modules/Saves/Internals/State.h"\n')
        self.write("Launcher/Engines/Engine0.1/EngineManifest.json", "{}")
        self.write("Launcher/Engines/Engine0.1/Core/Runtime/Game.cpp", '#include "Input/Internals/Reader.h"\n')
        self.assertEqual([entry.split(":")[0] for entry in self.check()], ["Engine/Core/Runtime/Game.cpp"])


if __name__ == "__main__":
    unittest.main()

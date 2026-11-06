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

    def test_simulation_stays_away_from_drawing_clocks_and_standard_random(self):
        self.write("Game/Simulation/Rules.cpp", '#include "Simulation/Simulation.h"\n#include "Utilities/Random/Rng.h"\n'
                   '#include "Tuning/Tuning.h"\n#include "Simulation/Rules.h"\n#include "Animation/Timeline.h"\n'
                   '#include "Animation/Easing.h"\n#include <vector>\n')
        self.assertEqual(self.check(), [])
        for include in ('"Physics/Debug/ColliderDraw.h"', '"Graphics/Paint/Painter.h"', '"Audio/AudioSystem.h"', '"Runtime/TimeScale.h"', "<random>", "<chrono>",
                        "<raylib.h>", '"Scene/Scene.h"'):
            with self.subTest(include=include):
                self.write("Game/Simulation/Rules.cpp", f"#include {include}\n")
                found = self.check()
                self.assertEqual(len(found), 1)
                self.assertIn("simulation code", found[0])
        self.write("Game/Simulation/Rules.cpp", "#pragma once\n")
        self.write("Game/Views/PlayerView.cpp", '#include "Graphics/Paint/Painter.h"\n#include <random>\n')
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

    def test_recipes_are_consumers_even_inside_engine(self):
        self.write("Engine/AgentDocs/Recipes/Code/Example.cpp",
                   '#include "Input/Internals/Reader.h"\n#include <Modules/Saves/Internals/State.h>\n')
        self.assertEqual(len(self.check()), 2)

    def test_recipes_cannot_include_raylib_with_either_delimiter(self):
        self.write("Engine/AgentDocs/Recipes/Code/Example.cpp", '#include <raylib.h>\n#include "rlgl.h"\n')
        self.assertEqual(len(self.check()), 2)

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

    def test_the_ui_layers_look_only_downwards(self):
        for path, include in (("Core/Ui/Layout/Solver.cpp", "Graphics/Paint/Painter.h"),
                              ("Core/Ui/Kernel/Ui.cpp", "Ui/Widgets/UiButton.h"),
                              ("Core/Ui/Kernel/Ui.h", "Ui/Looks/UiButtonLook.h"),
                              ("Core/Ui/Looks/UiButtonLook.h", "Ui/Widgets/UiButton.h"),
                              ("Core/Ui/Looks/UiButtonLook.cpp", "Input/Input.h"),
                              ("Core/Ui/Widgets/UiButton.cpp", "Graphics/Paint/Painter.h"),
                              ("Core/Ui/Widgets/UiButton.cpp", "Audio/SoundPlayer.h"),
                              ("Core/Ui/Kernel/UiKernel.cpp", "Audio/SoundKey.h"),
                              ("Core/Ui/Kernel/Ui.h", "Ui/Sound/UiSounds.h"),
                              ("Core/Ui/Looks/UiButtonLook.h", "Audio/SoundKey.h"),
                              ("Core/Ui/Sound/UiSounds.cpp", "Ui/Widgets/UiButton.h"),
                              ("Core/Ui/Sound/UiSounds.cpp", "Graphics/Paint/Painter.h")):
            with self.subTest(path=path, include=include):
                self.write(f"Engine/{path}", f'#include "{include}"\n')
                self.assertEqual(len(self.check()), 1, self.check())
                (self.engine / path).unlink()

    def test_the_ui_layers_may_use_what_is_below(self):
        self.write("Engine/Core/Ui/Layout/Solver.cpp", '#include "Ui/Layout/UiLayout.h"\n#include "Utilities/Geometry/Rect.h"\n')
        self.write("Engine/Core/Ui/Kernel/Ui.cpp", '#include "Ui/Layout/UiLayout.h"\n#include "Graphics/Paint/Painter.h"\n')
        self.write("Engine/Core/Ui/Looks/Look.cpp", '#include "Ui/Kernel/Ui.h"\n#include "Graphics/Paint/Painter.h"\n')
        self.write("Engine/Core/Ui/Widgets/Button.cpp", '#include "Ui/Looks/Look.h"\n#include "Ui/Kernel/Ui.h"\n')
        self.write("Engine/Core/Ui/Sound/Sounds.cpp", '#include "Ui/Kernel/Ui.h"\n#include "Audio/SoundKey.h"\n')
        self.assertEqual(self.check(), [])

    def test_a_simulation_cannot_include_the_sound_player_or_keys(self):
        for header in ("SoundPlayer.h", "SoundKey.h", "SoundRow.h", "SoundTables.h", "SoundOutput.h"):
            with self.subTest(header=header):
                self.write("Game/Simulation/Rules.cpp", f'#include "Audio/{header}"\n')
                self.assertIn("simulation code", self.check()[0])

    def test_the_camera_includes_only_what_a_simulation_may(self):
        self.write("Engine/Core/Camera/Camera2D.cpp", '#include "Camera/CameraView.h"\n#include "Animation/SmoothDamp.h"\n'
                   '#include "Utilities/Geometry/Float2.h"\n')
        self.write("Engine/Core/Camera/Debug/Guides.cpp", '#include "Graphics/Paint/Painter.h"\n')
        self.assertEqual(self.check(), [])
        self.write("Engine/Core/Camera/Camera2D.cpp", '#include "Graphics/Paint/Painter.h"\n')
        self.assertIn("Core/Camera includes", self.check()[0])

    def test_lighting_stands_on_paint_and_paint_does_not_know_it(self):
        self.write("Engine/Core/Graphics/Light/LightLayer.cpp", '#include "Graphics/Light/LightLayer.h"\n'
                   '#include "Graphics/Paint/Painter.h"\n#include "Graphics/Surface/Surface.h"\n'
                   '#include "Camera/CameraView.h"\n#include "Utilities/Color4.h"\n')
        self.write("Engine/Core/Graphics/Paint/Painter.cpp", '#include "Graphics/Surface/Surface.h"\n')
        self.assertEqual(self.check(), [])
        self.write("Engine/Core/Graphics/Atmosphere/Fog.cpp", '#include "Fx/FxStage.h"\n')
        self.assertIn("Core/Graphics/Atmosphere includes", self.check()[0])
        self.write("Engine/Core/Graphics/Atmosphere/Fog.cpp", '#include "Graphics/Paint/Painter.h"\n')
        self.write("Engine/Core/Weather/Sky.h", '#include "Graphics/Paint/Painter.h"\n')
        self.assertIn("Core/Weather includes", self.check()[0])
        self.write("Engine/Core/Weather/Sky.h", '#include "Animation/Pose.h"\n')
        self.write("Engine/Core/Graphics/Light/CollisionOccluders.cpp", '#include "Physics/Collision/Shape.h"\n')
        self.assertEqual(self.check(), [])
        self.write("Engine/Core/Graphics/Light/LightLayer.cpp", '#include "Physics/Collision/Shape.h"\n')
        self.assertIn("Core/Graphics/Light includes", self.check()[0])
        self.write("Engine/Core/Graphics/Light/LightLayer.cpp", '#include "Graphics/Paint/Painter.h"\n')
        self.write("Engine/Core/Graphics/Paint/Painter.cpp", '#include "Graphics/Light/LightLayer.h"\n')
        self.assertIn("Core/Graphics/Paint includes", self.check()[0])

    def test_a_simulation_cannot_include_lighting(self):
        self.write("Game/Simulation/Rules.cpp", '#include "Graphics/Light/LightLayer.h"\n')
        self.assertIn("simulation code", self.check()[0])

    def test_a_simulation_may_keep_a_camera_but_not_draw_its_guides(self):
        self.write("Game/Simulation/Rules.cpp", '#include "Camera/Camera2D.h"\n')
        self.assertEqual(self.check(), [])
        self.write("Game/Simulation/Rules.cpp", '#include "Camera/Debug/CameraGuides.h"\n')
        self.assertIn("simulation code", self.check()[0])

    def test_only_the_reflection_facade_includes_pfr(self):
        self.write("Engine/Core/Utilities/Reflection.h", '#include <pfr/core.hpp>\n#include <pfr/core_name.hpp>\n')
        self.assertEqual(self.check(), [])
        self.write("Engine/Core/Simulation/StateVisitor.h", '#include <pfr.hpp>\n')
        self.write("Game/Simulation/Run.h", '#include "pfr/core.hpp"\n')
        found = self.check()
        self.assertTrue(any("StateVisitor.h:1" in line and "Reflection.h" in line for line in found))
        self.assertTrue(any("Game/Simulation/Run.h:1" in line and "Reflection.h" in line for line in found))

    def test_text_shaping_stays_behind_text_internals(self):
        self.write("Engine/Core/Graphics/Text/Internals/TextShaper.cpp", '#include <kb_text_shape.h>\n')
        self.assertEqual(self.check(), [])
        self.write("Engine/Core/Graphics/Paint/PainterText.cpp", '#include <kb_text_shape.h>\n')
        found = self.check()
        self.assertTrue(any("PainterText.cpp:1" in line and "text shaping" in line for line in found))

    def test_the_audio_library_stays_behind_audio_internals(self):
        self.write("Engine/Core/Audio/Internals/Graph.cpp", '#include <miniaudio.h>\n#include <extras/stb_vorbis.c>\n')
        self.assertEqual(self.check(), [])
        self.write("Engine/Core/Runtime/Game.cpp", '#include <miniaudio.h>\n')
        self.write("Engine/Core/Audio/Player.h", '#include "miniaudio.h"\n')
        found = self.check()
        self.assertTrue(any("Runtime/Game.cpp:1" in line and "audio library" in line for line in found))
        self.assertTrue(any("Audio/Player.h:1" in line and "audio library" in line for line in found))

    def test_audio_does_not_use_raylib(self):
        self.write("Engine/Core/Audio/Internals/Device.cpp", '#include <raylib.h>\n')
        self.assertTrue(any("audio does not use raylib" in line for line in self.check()))

    def test_public_audio_headers_stand_low(self):
        self.write("Engine/Core/Audio/Sound.h", '#include "Audio/SoundBus.h"\n#include "Utilities/Geometry/Float2.h"\n'
                   '#include "Animation/Curve.h"\n')
        self.assertEqual(self.check(), [])
        self.write("Engine/Core/Audio/Other.h", '#include "Camera/CameraView.h"\n')
        self.assertTrue(any("Audio/Other.h:1" in line and "public audio header" in line for line in self.check()))

    def test_a_simulation_cannot_include_a_live_sound(self):
        self.write("Game/Simulation/Rules.cpp", '#include "Audio/LiveSound.h"\n')
        self.assertTrue(any("simulation code" in line for line in self.check()))

    def test_a_simulation_cannot_include_the_ui(self):
        self.write("Engine/Core/Simulation/Rules.cpp", '#include "Ui/Kernel/Ui.h"\n')
        self.assertIn("simulation code", self.check()[0])


if __name__ == "__main__":
    unittest.main()

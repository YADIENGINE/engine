"""`asset`: graphic skeletons, folders and moves that keep includes correct; no compiler."""

import tempfile
import unittest
from pathlib import Path

from Tools import cli
from Tools.commands.asset import make_folder, move_asset, new_graphic
from Tools.errors import ToolError

GRAPHICS = Path("Game/Assets/Graphics")


class AssetCommandTests(unittest.TestCase):
    def setUp(self):
        directory = tempfile.TemporaryDirectory()
        self.addCleanup(directory.cleanup)
        self.root = Path(directory.name)
        (self.root / GRAPHICS).mkdir(parents=True)

    def write(self, relative: str, text: str = "") -> Path:
        path = self.root / relative
        path.parent.mkdir(parents=True, exist_ok=True)
        path.write_text(text, encoding="utf-8", newline="")
        return path

    def test_parser(self):
        args = cli.build_parser().parse_args(["asset", "move", "Enemies/Goblin", "Monsters"])
        self.assertEqual((args.command, args.asset_action, args.source, args.destination),
                         ("asset", "move", "Enemies/Goblin", "Monsters"))

    def test_new_writes_a_skeleton_that_includes_its_own_path(self):
        answer = new_graphic(self.root, "Goblin", "Enemies")
        self.assertEqual(len(answer["created"]), 3)
        registration = (self.root / GRAPHICS / "Enemies/Goblin.asset.cpp").read_text(encoding="utf-8")
        self.assertIn('#include "Assets/Graphics/Enemies/Goblin.h"', registration)
        self.assertIn("RegisterGraphicAsset(&MakeGoblin)", registration)
        self.assertIn("void DrawGoblin(", (self.root / GRAPHICS / "Enemies/Goblin.cpp").read_text(encoding="utf-8"))

    def test_new_rejects_bad_names_paths_and_existing_assets(self):
        for name, folder in (("goblin", ""), ("Gob lin", ""), ("Goblin", "../Outside"), ("Goblin", "C:/x")):
            with self.subTest(name=name, folder=folder), self.assertRaises(ToolError):
                new_graphic(self.root, name, folder)
        new_graphic(self.root, "Goblin", "")
        with self.assertRaises(ToolError):
            new_graphic(self.root, "Goblin", "")

    def test_mkdir(self):
        make_folder(self.root, "Enemies/Small")
        self.assertTrue((self.root / GRAPHICS / "Enemies/Small").is_dir())
        with self.assertRaises(ToolError):
            make_folder(self.root, "Enemies/Small")

    def test_moving_an_asset_moves_its_files_and_updates_only_its_includes(self):
        new_graphic(self.root, "Goblin", "Enemies")
        new_graphic(self.root, "GoblinKing", "Enemies")
        make_folder(self.root, "Monsters")
        self.write(str(GRAPHICS / "Enemies/Goblin.tuning.json"), "{}")
        user = self.write(
            "Game/Scenes/Fight.cpp",
            '#include "Assets/Graphics/Enemies/Goblin.h"\r\n'
            '#include "Assets/Graphics/Enemies/Goblin.tuning.h"\n'
            '#include "Assets/Graphics/Enemies/GoblinKing.h"\n'
            'const char* text = "Assets/Graphics/Enemies/Goblin.h";\n')
        ignored = self.write("build/Generated.cpp", '#include "Assets/Graphics/Enemies/Goblin.h"\n')

        answer = move_asset(self.root, "Enemies/Goblin", "Monsters")

        self.assertTrue((self.root / GRAPHICS / "Monsters/Goblin.asset.cpp").is_file())
        self.assertTrue((self.root / GRAPHICS / "Monsters/Goblin.tuning.json").is_file())
        self.assertFalse((self.root / GRAPHICS / "Enemies/Goblin.h").exists())
        self.assertTrue((self.root / GRAPHICS / "Enemies/GoblinKing.h").is_file())
        with open(user, encoding="utf-8", newline="") as file:
            self.assertEqual(file.read(),
                             '#include "Assets/Graphics/Monsters/Goblin.h"\r\n'
                             '#include "Assets/Graphics/Monsters/Goblin.tuning.h"\n'
                             '#include "Assets/Graphics/Enemies/GoblinKing.h"\n'
                             'const char* text = "Assets/Graphics/Enemies/Goblin.h";\n')
        self.assertIn('Enemies/Goblin.h', ignored.read_text(encoding="utf-8"))
        self.assertIn("Game/Scenes/Fight.cpp", answer["includesUpdated"])
        moved = (self.root / GRAPHICS / "Monsters/Goblin.asset.cpp").read_text(encoding="utf-8")
        self.assertIn('#include "Assets/Graphics/Monsters/Goblin.h"', moved)

    def test_moving_a_folder_updates_includes_below_it(self):
        new_graphic(self.root, "Goblin", "Enemies/Small")
        make_folder(self.root, "Monsters")
        user = self.write("Editor/Scene.cpp", '#include "Assets/Graphics/Enemies/Small/Goblin.h"\n')
        move_asset(self.root, "Enemies", "Monsters")
        self.assertTrue((self.root / GRAPHICS / "Monsters/Enemies/Small/Goblin.h").is_file())
        self.assertIn("Monsters/Enemies/Small/Goblin.h", user.read_text(encoding="utf-8"))
        move_asset(self.root, "Monsters/Enemies", "")
        self.assertTrue((self.root / GRAPHICS / "Enemies/Small/Goblin.h").is_file())

    def test_move_refuses_what_it_cannot_do_and_changes_nothing(self):
        new_graphic(self.root, "Goblin", "Enemies")
        new_graphic(self.root, "Goblin", "Monsters")
        user = self.write("Game/Fight.cpp", '#include "Assets/Graphics/Enemies/Goblin.h"\n')
        make_folder(self.root, "Enemies/Inner")
        cases = (("Enemies/Goblin", "Monsters"), ("Enemies/Goblin", "Enemies"), ("Enemies", "Enemies/Inner"),
                 ("Missing", ""), ("Enemies/Goblin", "Missing"), ("../Goblin", ""))
        for source, destination in cases:
            with self.subTest(source=source, destination=destination), self.assertRaises(ToolError):
                move_asset(self.root, source, destination)
        self.assertTrue((self.root / GRAPHICS / "Enemies/Goblin.h").is_file())
        self.assertIn("Enemies/Goblin.h", user.read_text(encoding="utf-8"))


if __name__ == "__main__":
    unittest.main()

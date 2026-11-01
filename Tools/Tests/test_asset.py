"""`asset`: graphic skeletons, folders and moves that keep includes correct; no compiler."""

import contextlib
import io
import json
import tempfile
import unittest
from pathlib import Path

from Tools import cli
from Tools.commands.asset import make_folder, move_asset, new_graphic, unlisted_graphics
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

    def test_unlisted_scans_game_headers_and_checks_all_project_registrations(self):
        self.write("Game/Enemies.h", "#pragma once\nvoid DrawGoblin(Painter& paint);\n"
                   "void DrawGoblinKing(Painter &paint);\nvoid DrawPotion(\n    Painter& paint);\n")
        self.write("Game/More.hpp", "void DrawCoin(Painter&);\n")
        self.write("Game/OnlySource.cpp", "void DrawSourceOnly(Painter& paint) {}\n")
        self.write("Editor/Preview.h", "void DrawEditorOnly(Painter& paint);\n")
        self.write("Editor/Assets/Enemies.asset.cpp", "asset.draw = &DrawGoblin;\n")
        self.write("Engine/Assets/Default.asset.cpp", "DrawPotion\n")
        self.assertEqual(unlisted_graphics(self.root, []), [
            {"file": "Game/Enemies.h", "line": 3, "function": "DrawGoblinKing"},
            {"file": "Game/Enemies.h", "line": 4, "function": "DrawPotion"},
            {"file": "Game/More.hpp", "line": 1, "function": "DrawCoin"},
        ])

    def test_unlisted_ignores_members_comments_literals_and_other_signatures(self):
        self.write("Game/Draw.h", '/* void DrawComment(Painter&); */\n'
                   '// void DrawLineComment(Painter&);\n'
                   'const char* example = "void DrawString(Painter&);";\n'
                   'const char* raw = R"example(void DrawRaw(Painter&);)example";\n'
                   '#define DRAW void DrawMacro(Painter&);\n'
                   'struct View { void DrawMember(Painter&); static void DrawStatic(Painter&); };\n'
                   'void View::DrawQualified(Painter&) {}\n'
                   'int DrawResult(Painter&);\nvoid DrawOther(int, Painter&);\n'
                   'void DrawPointer(Painter*);\nvoid DrawRvalue(Painter&&);\n'
                   'namespace Shapes {\nvoid DrawFree(Painter &paint);\n}\n')
        self.assertEqual(unlisted_graphics(self.root, []), [
            {"file": "Game/Draw.h", "line": 13, "function": "DrawFree"}])

    def test_unlisted_limits_headers_but_finds_registration_in_unchanged_files(self):
        listed = self.write("Game/Listed.h", "void DrawListed(Painter&);\n")
        self.write("Game/Other.h", "void DrawOther(Painter&);\n")
        self.write("Game/Assets/Graphics/Listed.asset.cpp", "DrawListed(paint, box, time);\n")
        header = self.write("Editor/Preview.h", "void DrawPreview(Painter&);\n")
        self.assertEqual(unlisted_graphics(self.root, [str(listed), "Game/Deleted.h", "Game/Other.cpp"]), [])
        self.assertEqual(unlisted_graphics(self.root, [str(header), "Editor/Preview.h"]), [
            {"file": "Editor/Preview.h", "line": 1, "function": "DrawPreview"}])

    def test_unlisted_skips_excluded_folders_and_paths_outside_project(self):
        skipped = [f"{folder}/Hidden.h" for folder in ("build", "Engine", "Launcher", "Game/build", "Game/Engine", "Game/Launcher")]
        for path in skipped:
            self.write(path, "void DrawHidden(Painter&);\n")
        self.assertEqual(unlisted_graphics(self.root, []), [])
        self.assertEqual(unlisted_graphics(self.root, skipped + ["../Outside.h"]), [])

    def test_unlisted_cli_prints_json_and_returns_zero_with_or_without_findings(self):
        self.write("Project.json", '{"version": 1, "name": "MyGame"}')
        new_graphic(self.root, "Goblin", "Enemies")
        self.write("Game/Loose.h", "void DrawLoose(Painter&);\n")
        for files, expected in (([], [{"file": "Game/Loose.h", "line": 1, "function": "DrawLoose"}]),
                                (["Game/Assets/Graphics/Enemies/Goblin.h"], [])):
            with self.subTest(files=files), contextlib.redirect_stdout(io.StringIO()) as output:
                code = cli.main(["--project", str(self.root), "asset", "unlisted", *files])
            self.assertEqual(code, 0)
            self.assertEqual(json.loads(output.getvalue()), expected)


if __name__ == "__main__":
    unittest.main()

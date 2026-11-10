"""Settings files (Docs/ProjectSettings.md): shared cases (the C++ side runs them too), generated code, `check`."""

import json
import tempfile
import unittest
from pathlib import Path

from Tools.tuning import settings
from Tools.tuning.document import TuningError, read_fields

CASES = Path(__file__).resolve().parent / "SettingsCases"


def _read(kind: str, text: str):
    if kind == "schema":
        return read_fields(text, schema=True)
    if kind == "tuning":
        return read_fields(text)
    return settings.read_values(text, settings.read_schema())


class SharedCaseTests(unittest.TestCase):
    def test_cases_match_the_expected_error_line(self):
        cases = json.loads((CASES / "cases.json").read_text(encoding="utf-8"))["cases"]
        self.assertTrue(cases)
        for case in cases:
            with self.subTest(case["file"]):
                text = (CASES / case["file"]).read_text(encoding="utf-8")
                if case["line"] is None:
                    _read(case["kind"], text)
                    continue
                with self.assertRaises(TuningError) as raised:
                    _read(case["kind"], text)
                self.assertEqual(raised.exception.line, case["line"], raised.exception.text)


class ValuesTests(unittest.TestCase):
    def test_unset_values_are_the_defaults(self):
        values = settings.read_values('{ "canvasWidth": 640 }', settings.read_schema())
        self.assertEqual(values["canvasWidth"], 640)
        self.assertEqual(values["canvasHeight"], 720)
        self.assertEqual(values["title"], "Game")

    def test_canonical_follows_the_schema_order_and_keeps_spelling(self):
        text = '{"canvasWidth": 6.4e2, "title":"A"}'
        with self.assertRaises(TuningError):  # An int is written as an integer
            settings.canonical_values(text, settings.read_schema())
        text = '{"canvasWidth": 640, "pixelUi": true, "artPixel": 3, "title":"A"}'
        self.assertEqual(settings.canonical_values(text, settings.read_schema()),
                         '{\n  "title": "A",\n  "canvasWidth": 640,\n  "artPixel": 3,\n  "pixelUi": true\n}\n')
        self.assertEqual(settings.canonical_values("{ }", settings.read_schema()), "{}\n")


class GeneratedCodeTests(unittest.TestCase):
    def test_committed_header_matches_the_schema(self):
        self.assertIsNone(settings.header_problem(fix=False))

    def test_values_file_sets_only_what_differs(self):
        values = settings.read_values('{ "title": "Café", "canvasWidth": 1280 }', settings.read_schema())
        text = settings.render_values(settings.read_schema(), values, "Game/Game.settings.json")
        self.assertIn('settings.title = "Caf\\303\\251";', text)
        self.assertNotIn("canvasWidth", text)  # Equal to the default
        self.assertIn("RegisterProjectSettings(Compiled(), true)", text)

    def test_resources_carry_the_icon_and_version(self):
        with tempfile.TemporaryDirectory() as folder:
            project = Path(folder)
            (project / "Game").mkdir()
            (project / "Game" / "Icon.ico").write_bytes(b"\0")
            text = '{ "title": "My \\"Game\\"", "version": "2.1", "icon": "Game/Icon.ico" }'
            values = settings.read_values(text, settings.read_schema())
            rc = settings.render_rc(values, project, settings.value_lines(text, settings.read_schema()), "x")
            self.assertIn('GLFW_ICON ICON "' + (project / "Game" / "Icon.ico").resolve().as_posix() + '"', rc)
            self.assertIn("FILEVERSION 2,1,0,0", rc)
            self.assertIn('VALUE "ProductName", "My ""Game"""', rc)
            self.assertNotIn("CompanyName", rc)  # Empty: left out

    def test_missing_icon_stops_the_build(self):
        with tempfile.TemporaryDirectory() as folder:
            text = '{\n  "icon": "Game/Missing.ico"\n}\n'
            values = settings.read_values(text, settings.read_schema())
            with self.assertRaises(TuningError) as raised:
                settings.render_rc(values, Path(folder), settings.value_lines(text, settings.read_schema()), "x")
            self.assertEqual(raised.exception.line, 2)

    def test_absent_file_generates_the_defaults(self):
        with tempfile.TemporaryDirectory() as folder:
            out = Path(folder)
            settings.generate(out / "Game.settings.json", out / "a.cpp", out / "a.rc", out)
            self.assertNotIn("settings.", (out / "a.cpp").read_text(encoding="utf-8").split("ProjectSettings settings;")[1]
                             .split("return")[0])
            self.assertIn("FILEVERSION 1,0,0,0", (out / "a.rc").read_text(encoding="utf-8"))


class CheckTests(unittest.TestCase):
    def test_reports_and_fixes_the_project_file(self):
        with tempfile.TemporaryDirectory() as folder:
            root = Path(folder)
            (root / "Game").mkdir()
            values = root / "Game" / "Game.settings.json"
            values.write_text('{"canvasWidth": 640, "title": "A"}', encoding="utf-8")
            problems, fixed = settings.check_settings(root, fix=False)
            self.assertEqual(len(problems), 1)
            problems, fixed = settings.check_settings(root, fix=True)
            self.assertEqual((problems, fixed), ([], ["Game/Game.settings.json"]))
            self.assertTrue(values.read_text(encoding="utf-8").startswith('{\n  "title": "A",'))
            values.write_text('{\n  "bogus": 1\n}\n', encoding="utf-8")
            problems, _ = settings.check_settings(root, fix=True)
            self.assertEqual(problems, ['Game/Game.settings.json:2: unknown setting "bogus"'])


if __name__ == "__main__":
    unittest.main()

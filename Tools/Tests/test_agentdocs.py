"""Exercise extraction, snippets, skills, links, task routes and API names with tiny files, independent of the real
engine."""

import json
import tempfile
import unittest
from pathlib import Path
from unittest.mock import patch

from Tools import cli
from Tools.agentdocs.api import declared_name, parse_header, public_headers, sentence
from Tools.agentdocs.check import check_agentdocs, check_skills
from Tools.agentdocs.links import anchor, check_links
from Tools.agentdocs.names import check_names
from Tools.agentdocs.tasks import check_tasks, load_tasks

TASKS = {'tasks': [{'task': 'Run an example', 'files': ['Core/Example.h'], 'recipe': 'AgentDocs/Recipes/Example.md',
                    'docs': ['Docs/Example.md#use'], 'words': ['demo']}]}


def write(root: Path, path: str, text: str):
    target = root / path
    target.parent.mkdir(parents=True, exist_ok=True)
    target.write_text(text, encoding='utf-8', newline='\n')


class ParserTests(unittest.TestCase):
    def fixture(self, name):
        path = Path(__file__).with_name('AgentDocsCases') / name
        return parse_header(f'Core/{name}', path.read_text(encoding='utf-8'))

    def test_visibility_comments_and_inline_bodies(self):
        fixture = Path(__file__).with_name('AgentDocsCases') / 'Public.h'
        header = parse_header('Core/Widget.h', fixture.read_text(encoding='utf-8'))
        signatures = '\n'.join(d.signature for d in header.declarations)
        self.assertIn('Widget', signatures)
        self.assertIn('Count() const', signatures)
        self.assertIn('void Set(int count)', signatures)
        self.assertIn('constexpr int Limit', signatures)
        self.assertIn('enum class Mode', signatures)
        self.assertIn('Identity(T value)', signatures)
        self.assertNotIn('Secret', signatures)
        self.assertNotIn('Hook', signatures)
        self.assertNotIn('return', signatures)
        count = next(d for d in header.declarations if 'Count()' in d.signature)
        self.assertEqual(count.comment, 'Return its count.')
        self.assertEqual(header.summary, '')

    def test_forward_declarations_are_omitted_but_definitions_remain(self):
        header = self.fixture('ForwardDeclarations.h')
        self.assertEqual([d.signature for d in header.declarations],
                         ['class Defined', 'struct Nested', 'int value = 0'])
        self.assertEqual(header.skipped, 0)

    def test_special_members_and_deleted_functions_are_omitted(self):
        header = self.fixture('SpecialMembers.h')
        self.assertEqual([d.signature for d in header.declarations], [
            'struct Record', 'Record() = default', 'explicit Record(int value)',
            'Record& operator=(int value)', 'bool operator==(const Record&) const = default',
            'template<class T> struct Generic', 'explicit Generic(const T& value)',
        ])
        self.assertEqual(header.skipped, 0)

    def test_file_level_summary_is_not_a_declaration_comment(self):
        header = self.fixture('FileSummary.h')
        self.assertEqual(header.summary, 'File-level helpers for navigation fixtures.')
        self.assertEqual(header.declarations[0].comment, 'A record with its own declaration comment.')

    def test_attached_comment_is_only_a_declaration_comment(self):
        header = self.fixture('DeclarationSummary.h')
        self.assertEqual(header.summary, '')
        self.assertEqual(header.declarations[0].comment, 'This describes the declaration, not the file.')

    def test_later_comment_is_never_borrowed_as_file_summary(self):
        header = self.fixture('NoSummary.h')
        self.assertEqual(header.summary, '')
        self.assertEqual(header.declarations[0].comment, 'This describes a later declaration.')

    def test_lines_names_and_type_ranges(self):
        fixture = Path(__file__).with_name('AgentDocsCases') / 'Public.h'
        header = parse_header('Core/Widget.h', fixture.read_text(encoding='utf-8'))
        found = {(d.owner, d.name): d for d in header.declarations}
        widget = found[('', 'Widget')]
        self.assertEqual((widget.line, widget.end), (3, 18))  # `#pragma once` still counts as a line
        self.assertEqual((found[('Widget', 'Widget')].line, found[('Widget', 'Widget')].end), (6, 0))
        self.assertEqual(found[('Widget', 'Count')].line, 8)
        self.assertEqual((found[('Widget', 'Result')].line, found[('Widget', 'Result')].end), (12, 12))
        self.assertEqual(found[('Widget::Result', 'value')].line, 12)
        self.assertEqual(found[('', 'Identity')].line, 20)
        self.assertEqual((found[('', 'Mode')].line, found[('', 'Mode')].end), (22, 22))
        self.assertTrue(widget.is_type and not found[('Widget', 'Set')].is_type)
        self.assertEqual(found[('', 'Mode')].values, [('First', 22), ('Second', 22)])
        # Not API, kept to say why a name is missing; members of a private nested class are not kept at all
        self.assertEqual([(d.name, d.access, d.line) for d in header.hidden],
                         [('Hook', 'protected', 14), ('_count', 'private', 16), ('Hidden', 'private', 17)])

    def test_declared_names(self):
        cases = {
            '[[nodiscard]] constexpr int StepsIn(double seconds, int stepsPerSecond)': 'StepsIn',
            'template<class T, std::size_t N> constexpr T StepChoice(T current, const std::array<T, N>& c)': 'StepChoice',
            'template<AnimationPose PoseType, class Position> class BasicAnimator': 'BasicAnimator',
            'class WorkerScene : public Scene': 'WorkerScene',
            'struct [[nodiscard]] PaintScope': 'PaintScope',
            'enum class Mode { First, Second }': 'Mode',
            'using UiMeasure = std::function<Float2(float maxWidth)>': 'UiMeasure',
            'template<class P> concept AnimationPose = std::copyable<P>': 'AnimationPose',
            'std::function<void(const ColliderView&)> visit': 'visit',
            'UiInsets padding = UiInsets::Symmetric(20.0f, 8.0f)': 'padding',
            'static constexpr int Count{3}': 'Count',
            'float values[4]': 'values',
            'inline constexpr SoundOverlap SoundOverlap::Skip = SoundOverlap(0)': 'Skip',
            'bool operator==(const Vec2&) const = default': 'operator==',
            'Camera2D& operator [](std::size_t index)': 'operator[]',
            '[[nodiscard]] explicit operator bool() const': 'operator bool',
            'explicit Widget(int count = 2)': 'Widget',
        }
        for signature, name in cases.items():
            self.assertEqual(declared_name(signature), name, signature)

    def test_incomplete_declaration_is_counted(self):
        self.assertEqual(parse_header('Core/X.h', 'template <').skipped, 1)

    def test_first_sentence_keeps_abbreviations(self):
        self.assertEqual(sentence('A point (e.g. a cursor). Update it.'), 'A point (e.g. a cursor).')

    def test_multiline_defaults_attributes_and_braced_values(self):
        header = parse_header('Core/Values.h', '''
struct Values {
    // A callback.
    [[nodiscard]] int Read(int n = 1) const { return n; }
    static constexpr int Count{3};
private:
    void Hidden() {}
};
''')
        signatures = '\n'.join(d.signature for d in header.declarations)
        self.assertIn('Read(int n = 1) const', signatures)
        self.assertNotIn('return', signatures)
        self.assertIn('Count', signatures)
        self.assertNotIn('Hidden', signatures)

    def test_trailing_comments_stay_with_fields_and_enums_keep_values(self):
        header = parse_header('Core/Value.h', '''
struct Value {
    Value(int value) : _value{value} {}
    int first = 1; // First field.
    int second = 2;
private:
    int _value;
};
enum class Kind { One, Two };
''')
        first = next(d for d in header.declarations if 'first' in d.signature)
        second = next(d for d in header.declarations if 'second' in d.signature)
        self.assertEqual(first.comment, 'First field.')
        self.assertEqual(second.comment, '')
        self.assertTrue(any(d.signature == 'Value(int value)' for d in header.declarations))
        self.assertTrue(any('One, Two' in d.signature for d in header.declarations))
        self.assertEqual(header.summary, '')

    def test_operators_do_not_capture_bodies(self):
        header = parse_header('Core/Value.h', '''
struct Value {
    Value& operator+=(int n) { _value += n; return *this; }
    bool operator==(const Value&) const = default;
private:
    int _value = 0;
};
''')
        signatures = '\n'.join(d.signature for d in header.declarations)
        self.assertIn('operator+=(int n)', signatures)
        self.assertIn('operator==(const Value&) const = default', signatures)
        self.assertNotIn('return', signatures)


class FreshnessTests(unittest.TestCase):
    def setUp(self):
        self.directory = tempfile.TemporaryDirectory()
        self.addCleanup(self.directory.cleanup)
        self.root = Path(self.directory.name)
        self.write('Core/Example.h', '// Example.\nstruct Example { void Run(); };\n')
        self.write('AgentDocs/Recipes/Code/Example.cpp',
                   '// [recipe:Example]\nvoid Example() {}\n// [/recipe]\n')
        self.write('AgentDocs/Recipes/Example.md',
                   'Use this example.\n<!-- recipe:Example -->\n```cpp\nold\n```\n<!-- /recipe -->\n')
        self.write('Docs/Example.md', '# Example\n\n## Use\n')
        self.write('AgentDocs/Tasks.json', json.dumps(TASKS))

    def write(self, path, text):
        write(self.root, path, text)

    def test_check_is_read_only_and_fix_is_idempotent(self):
        self.assertEqual(check_agentdocs(self.root)[0], ['AgentDocs/Recipes/Example.md: stale snippet (fix: check --fix)'])
        self.assertIn('old', (self.root / 'AgentDocs/Recipes/Example.md').read_text(encoding='utf-8'))
        self.assertEqual(check_agentdocs(self.root, True), ([], ['AgentDocs/Recipes/Example.md']))
        self.assertEqual(check_agentdocs(self.root), ([], []))
        self.assertEqual(check_agentdocs(self.root, True), ([], []))
        self.write('Core/Example.h', '// Example.\nstruct Example { void NewApi(); };\n')
        self.assertEqual(check_agentdocs(self.root), ([], []))  # Declarations are read live, never stored
        self.assertFalse((self.root / 'AgentDocs/Api').exists())

    def test_tasks_table_is_required(self):
        check_agentdocs(self.root, True)
        (self.root / 'AgentDocs/Tasks.json').unlink()
        self.assertEqual(check_agentdocs(self.root)[0], ['AgentDocs/Tasks.json: missing'])

    def test_missing_regions_and_duplicates_fail(self):
        check_agentdocs(self.root, True)
        self.write('AgentDocs/Recipes/Code/Other.cpp',
                   '// [recipe:Example]\nother\n// [/recipe]\n// [recipe:Unused]\nx\n// [/recipe]\n')
        problems, _ = check_agentdocs(self.root)
        self.assertTrue(any('duplicate region' in p for p in problems))
        self.assertTrue(any('no Markdown snippet' in p for p in problems))
        self.write('AgentDocs/Recipes/Code/Example.cpp', '')
        self.write('AgentDocs/Recipes/Code/Other.cpp', '')
        self.assertTrue(any('missing code region' in p for p in check_agentdocs(self.root)[0]))

    def test_skills_show_regions_and_their_json_must_parse(self):
        check_agentdocs(self.root, True)
        self.write('AgentDocs/Agent.md', '| x | `AgentDocs/Skills/demo/SKILL.md` |\n')
        self.write('AgentDocs/Skills/demo/SKILL.md', '---\nname: demo\ndescription: Demo.\n---\n\nNo example.\n')
        self.assertEqual(check_agentdocs(self.root)[0], [])  # A skill need not show a region
        self.write('AgentDocs/Recipes/Code/Short.cpp', '// [recipe:Short]\nvoid Short() {}\n// [/recipe]\n')
        self.write('AgentDocs/Skills/demo/SKILL.md', '---\nname: demo\ndescription: Demo.\n---\n\n'
                   '<!-- recipe:Short -->\n```cpp\nold\n```\n<!-- /recipe -->\n\n```json\n{ "run": 1, }\n```\n')
        problems = check_agentdocs(self.root)[0]
        self.assertTrue(any('SKILL.md: stale' in p for p in problems))
        self.assertTrue(any('invalid JSON example' in p for p in problems))
        check_agentdocs(self.root, True)
        self.assertIn('void Short() {}', (self.root / 'AgentDocs/Skills/demo/SKILL.md').read_text(encoding='utf-8'))

    def test_header_scope(self):
        for path in ('Core/Tests/Hidden.h', 'Core/Example/Internals/Hidden.h', 'ToolUi/Hidden.h',
                     'Terminal/Hidden.h', 'Editor/Worker/Private.h', 'Editor/Api/Visible.h'):
            self.write(path, 'struct Fixture {};\n')
        paths = [p.relative_to(self.root).as_posix() for p in public_headers(self.root)]
        self.assertEqual(paths, ['Core/Example.h', 'Editor/Api/Visible.h'])

    def test_cli_checks_engine_docs_from_a_consuming_project(self):
        with patch('Tools.cli.ENGINE_ROOT', self.root), patch('Tools.cli.violations', return_value=[]), \
                patch('Tools.cli.check_tuning', return_value=([], [])):
            self.assertEqual(cli.main(['--project', str(self.root.parent), 'check']), 1)
            self.assertEqual(cli.main(['--project', str(self.root.parent), 'check', '--fix']), 0)
            self.assertEqual(cli.main(['--project', str(self.root.parent), 'check']), 0)


class SkillTests(unittest.TestCase):
    def setUp(self):
        self.temp = tempfile.TemporaryDirectory()
        self.root = Path(self.temp.name)
        self.parser = cli.build_parser()

    def tearDown(self):
        self.temp.cleanup()

    def write(self, path, text):
        target = self.root / path
        target.parent.mkdir(parents=True, exist_ok=True)
        target.write_text(text, encoding='utf-8')

    def skill(self, folder, name=None, description='Do one thing.'):
        self.write(f'AgentDocs/Skills/{folder}/SKILL.md',
                   f'---\nname: {name or folder}\ndescription: {description}\n---\n\n# Body\n')

    def test_a_listed_skill_with_valid_front_matter_passes(self):
        self.skill('editor')
        self.write('AgentDocs/Agent.md', '| show | `AgentDocs/Skills/editor/SKILL.md` |\n')
        self.assertEqual(check_skills(self.root, self.parser), [])

    def test_front_matter_name_and_listing_are_required(self):
        self.skill('editor', name='viewer')
        self.skill('empty', description='')
        self.write('AgentDocs/Skills/bare/SKILL.md', '# No front matter\n')
        self.write('AgentDocs/Agent.md', '`AgentDocs/Skills/editor/SKILL.md` `AgentDocs/Skills/empty/SKILL.md`\n')
        problems = '\n'.join(check_skills(self.root, self.parser))
        self.assertIn('editor/SKILL.md: name "viewer" differs', problems)
        self.assertIn('empty/SKILL.md: description must have', problems)
        self.assertIn('bare/SKILL.md: front matter', problems)
        self.assertIn('no row for AgentDocs/Skills/bare/SKILL.md', problems)

    def test_tool_calls_must_exist(self):
        self.write('AgentDocs/Recipes/Use.md', 'Run `python Engine build`, `python Engine ctl find x` and\n'
                   '`python Engine ctl teleport`, then `python Engine launch`.\n')
        problems = check_skills(self.root, self.parser)
        self.assertEqual(problems, ['AgentDocs/Recipes/Use.md:2: unknown action "python Engine ctl teleport"',
                                    'AgentDocs/Recipes/Use.md:2: unknown command "python Engine launch"'])


class LinkTests(unittest.TestCase):
    def setUp(self):
        self.temp = tempfile.TemporaryDirectory()
        self.root = Path(self.temp.name)
        self.write('Docs/Ui.md', '# UI\n\n## Sounds\n\n## Editor/Main.cpp\n\n```md\n## Not a heading\n```\n## Sounds\n')
        self.write('Core/Ui/Ui.h', '')
        self.write('AgentDocs/Recipes/Menu.md', '# Menu\n')

    def tearDown(self):
        self.temp.cleanup()

    def write(self, path, text):
        target = self.root / path
        target.parent.mkdir(parents=True, exist_ok=True)
        target.write_text(text, encoding='utf-8', newline='\n')

    def test_github_anchors(self):
        self.assertEqual(anchor('Editor/Main.cpp'), 'editormaincpp')
        self.assertEqual(anchor('1. Directory structure'), '1-directory-structure')
        self.assertEqual(anchor('`Ui` and **looks**'), 'ui-and-looks')
        self.assertEqual(anchor('D31. Game UI'), 'd31-game-ui')

    def test_existing_files_and_headings_pass(self):
        self.write('AgentDocs/Guide.md',
                   '# Index\n'
                   '| `Core/Ui/Ui.h` | `Recipes/Menu.md` | `Docs/Ui.md#sounds` | `Engine/Docs/Ui.md#sounds-1` |\n'
                   '[UI](../Docs/Ui.md#editormaincpp), [top](#index), [web](https://example.com/x#y)\n'
                   'Placeholders: `AgentDocs/Skills/<name>/SKILL.md`; project files: `Game/Main.cpp`, `Editor/Main.cpp`.\n')
        self.assertEqual(check_links(self.root), [])

    def test_missing_files_and_headings_fail(self):
        self.write('AgentDocs/Guide.md', 'See `Docs/Gone.md`, `Recipes/Gone.md`, `Core/Ui/Gone.h`,\n'
                   '`Docs/Ui.md#gone`, `Docs/Ui.md#not-a-heading` and [x](../Docs/Ui.md#gone).\n'
                   '```cpp\n// `Docs/InCode.md` is code, not a route\n```\n')
        problems = check_links(self.root)
        self.assertEqual(len(problems), 6, problems)
        self.assertTrue(all(p.startswith('AgentDocs/Guide.md:') for p in problems))
        self.assertEqual(sum('heading' in p for p in problems), 3)

    def test_files_left_out_of_released_copies_fail(self):
        self.write('.gitattributes', '* text=auto\n/Docs/Design export-ignore\n')
        self.write('Docs/Design/Assets.md', '# Assets\n\n## Rule\n')
        self.write('AgentDocs/Guide.md', 'See `Docs/Design/Assets.md#rule` and [rule](../Docs/Design/Assets.md#rule).\n')
        problems = check_links(self.root)
        self.assertEqual(len(problems), 2, problems)
        self.assertTrue(all('export-ignore' in p for p in problems))


class TasksTests(unittest.TestCase):
    def setUp(self):
        self.temp = tempfile.TemporaryDirectory()
        self.addCleanup(self.temp.cleanup)
        self.root = Path(self.temp.name)
        write(self.root, 'Core/Example.h', 'struct Example {};\n')
        write(self.root, 'Core/Example/Internals/Hidden.h', 'struct Hidden {};\n')
        write(self.root, 'Tools/commands/tool.py', '')
        write(self.root, 'AgentDocs/Recipes/Example.md', '# Example\n')
        write(self.root, 'Docs/Example.md', '# Example\n\n## Use\n')

    def tasks(self, *tasks):
        write(self.root, 'AgentDocs/Tasks.json', json.dumps({'tasks': list(tasks)}))

    def test_valid_routes_pass_and_load(self):
        self.tasks(TASKS['tasks'][0], {'task': 'Run a tool', 'files': ['Tools/commands/tool.py'], 'recipe': None,
                                       'docs': ['Docs/Example.md']})
        self.assertEqual(check_tasks(self.root), [])
        loaded = load_tasks(self.root)
        self.assertEqual([task.words for task in loaded], [['demo'], []])
        self.assertIsNone(loaded[1].recipe)

    def test_dead_routes_and_bad_fields_fail(self):
        self.tasks({'task': 'Broken', 'files': ['Core/Gone.h', 'Core/Example/Internals/Hidden.h'],
                    'recipe': 'Recipes/Example.md', 'docs': ['Docs/Example.md#gone'], 'words': ['Upper'], 'extra': 1},
                   {'task': 'Broken', 'files': [], 'docs': ['Docs/Example.md']},
                   'not an object')
        problems = '\n'.join(check_tasks(self.root))
        self.assertIn('task 1 "Broken": unknown field(s) extra', problems)
        self.assertIn('"Core/Gone.h" points at a file that does not exist', problems)
        self.assertIn('"Core/Example/Internals/Hidden.h" is not a public header', problems)
        self.assertIn('"recipe" must be null or AgentDocs/Recipes/<Name>.md', problems)
        self.assertIn('"Docs/Example.md#gone" points at a heading that does not exist', problems)
        self.assertIn('"words" must be a list of lower-case words', problems)
        self.assertIn('task 2 "Broken": duplicate task', problems)
        self.assertIn('task 2 "Broken": "files" must be a non-empty list', problems)
        self.assertIn('task 3: expected an object', problems)

    def test_invalid_json_and_shape_fail(self):
        write(self.root, 'AgentDocs/Tasks.json', '{"tasks": [}')
        self.assertTrue(check_tasks(self.root)[0].startswith('AgentDocs/Tasks.json: invalid JSON'))
        write(self.root, 'AgentDocs/Tasks.json', '[]')
        self.assertEqual(check_tasks(self.root), ['AgentDocs/Tasks.json: expected an object with one "tasks" list'])


class NamesTests(unittest.TestCase):
    def setUp(self):
        self.temp = tempfile.TemporaryDirectory()
        self.addCleanup(self.temp.cleanup)
        self.root = Path(self.temp.name)
        write(self.root, 'Core/Timeline.h', '// Not a name: GoneInComment\nint StepsIn(double s);\n'
                                            'struct CollisionWorld { void Sweep(); };\n')
        write(self.root, 'AgentDocs/Recipes/Code/Menu.cpp', 'struct UiMenuScreen {};\n')

    def test_known_names_pass_and_unknown_ones_fail(self):
        write(self.root, 'AgentDocs/Skills/demo/SKILL.md',
              'Use `StepsIn`, `CollisionWorld::Sweep`, `UiMenuScreen` and `DrawX`.\n'
              'Paths and commands are not names: `Core/GoneHeader.h`, `python Engine api GoneName`, `GoneFile.json`.\n'
              'Quoted text and lower-case forms are not names: `ui.Text("GoneText")`, `settings.buttons`.\n'
              '```cpp\nGoneInCode();\n```\n'
              'Unknown: `StepIn`, `CollisionWorld::Swept`, `GoneInComment`.\n')
        problems = check_names(self.root)
        self.assertEqual([p.split('`')[1] for p in problems], ['StepIn', 'Swept', 'GoneInComment'])
        self.assertTrue(all(p.startswith('AgentDocs/Skills/demo/SKILL.md:7: ') for p in problems))

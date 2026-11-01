"""Exercise extraction and freshness with tiny files, independent of the real generated output."""

import tempfile
import unittest
from pathlib import Path
from unittest.mock import patch

from Tools import cli
from Tools.agentdocs.api import generate, parse_header, public_headers, sentence
from Tools.agentdocs.check import check_agentdocs, check_skills


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

    def test_no_summary_placeholder_is_generated(self):
        with tempfile.TemporaryDirectory() as directory:
            root = Path(directory)
            (root / 'Core').mkdir()
            (root / 'Core/Example.h').write_text('// A declaration.\nstruct Example {};\n', encoding='utf-8')
            content = next(iter(generate(root)[0].values()))
        self.assertNotIn('_No leading', content)
        self.assertEqual(content.count('A declaration.'), 1)

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

    def write(self, path, text):
        target = self.root / path
        target.parent.mkdir(parents=True, exist_ok=True)
        target.write_text(text, encoding='utf-8', newline='\n')

    def test_check_is_read_only_and_fix_is_idempotent(self):
        self.assertTrue(check_agentdocs(self.root)[0])
        self.assertFalse((self.root / 'AgentDocs/Api').exists())
        self.assertFalse(check_agentdocs(self.root, True)[0])
        self.assertEqual(check_agentdocs(self.root), ([], []))
        self.assertEqual(check_agentdocs(self.root, True), ([], []))
        self.write('Core/Example.h', '// Example.\nstruct Example { void NewApi(); };\n')
        self.assertTrue(check_agentdocs(self.root)[0])
        self.assertTrue(check_agentdocs(self.root, True)[1])

    def test_obsolete_pages_missing_regions_and_duplicates_fail(self):
        check_agentdocs(self.root, True)
        self.write('AgentDocs/Api/Removed.md', 'obsolete\n')
        self.assertTrue(check_agentdocs(self.root)[0])
        check_agentdocs(self.root, True)
        self.assertFalse((self.root / 'AgentDocs/Api/Removed.md').exists())
        self.write('AgentDocs/Recipes/Code/Other.cpp',
                   '// [recipe:Example]\nother\n// [/recipe]\n// [recipe:Unused]\nx\n// [/recipe]\n')
        problems, _ = check_agentdocs(self.root)
        self.assertTrue(any('duplicate region' in p for p in problems))
        self.assertTrue(any('no Markdown snippet' in p for p in problems))
        self.write('AgentDocs/Recipes/Code/Example.cpp', '')
        self.write('AgentDocs/Recipes/Code/Other.cpp', '')
        self.assertTrue(any('missing code region' in p for p in check_agentdocs(self.root)[0]))

    def test_header_scope_and_stable_output(self):
        for path in ('Core/Tests/Hidden.h', 'Core/Example/Internals/Hidden.h', 'ToolUi/Hidden.h',
                     'Terminal/Hidden.h', 'Editor/Worker/Private.h', 'Editor/Api/Visible.h'):
            self.write(path, 'struct Fixture {};\n')
        paths = [p.relative_to(self.root).as_posix() for p in public_headers(self.root)]
        self.assertEqual(paths, ['Core/Example.h', 'Editor/Api/Visible.h'])
        pages = generate(self.root)[0]
        self.assertIn('## `Core/Example.h`\n', pages['Example.h.md'])
        self.assertIn('## `Editor/Api/Visible.h`\n', pages['Editor.md'])
        self.assertTrue(all('](../../' not in text for text in pages.values()))
        self.assertEqual(generate(self.root)[0], generate(self.root)[0])
        self.assertTrue(all('\r' not in text for text in generate(self.root)[0].values()))

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

"""`python Engine api`: name levels, type listings and their cut, words ranking, paths and exit codes, on a tiny
engine."""

import contextlib
import io
import json
import tempfile
import unittest
from pathlib import Path
from unittest.mock import patch

from Tools import cli
from Tools.agentdocs import query
from Tools.agentdocs.query import answer, terms
from Tools.commands.api import display_prefix

WORLD = '''#pragma once
// Finds colliders fast.
class CollisionWorld {
public:
    CollisionWorld();
    // Moves a collider.
    void Move(int id);
    // The nearest collider a moving shape reaches.
    int Sweep(int shape) const;
    int Sweep(int shape, int mask) const;
    struct Hit {
        float point = 0.0f;
    };
private:
    int _count = 0;
};
'''
TIMELINE = '''#pragma once
// Seconds as whole steps at a step rate.
constexpr int StepsIn(double seconds, int stepsPerSecond) { return 0; }
// Fades the screen to black between two scenes.
void FadeScreen(float seconds);
'''
GAME = '''#pragma once
// Runtime owns the loop.
class Game {
public:
    // The game's sound tables.
    int& Sounds();
protected:
    // This frame's real duration.
    float RealSeconds() const;
private:
    int _frames = 0;
};
enum class InputKey { A, Space, Enter = 4 };
'''
TASKS = {'tasks': [
    {'task': 'Add a scene or fade between scenes', 'files': ['Core/Animation/Timeline.h'], 'recipe': None,
     'docs': ['Docs/Runtime.md#scenes'], 'words': ['transition']},
    {'task': 'Stop fast bullets', 'files': ['Core/Physics/CollisionWorld.h'], 'recipe': 'AgentDocs/Recipes/Hit.md',
     'docs': ['Docs/Collision.md']},
]}


class ApiQueryTests(unittest.TestCase):
    def setUp(self):
        self.temp = tempfile.TemporaryDirectory()
        self.addCleanup(self.temp.cleanup)
        self.root = Path(self.temp.name)
        self.write('Core/Physics/CollisionWorld.h', WORLD)
        self.write('Core/Animation/Timeline.h', TIMELINE)
        self.write('AgentDocs/Tasks.json', json.dumps(TASKS))

    def write(self, path, text):
        target = self.root / path
        target.parent.mkdir(parents=True, exist_ok=True)
        target.write_text(text, encoding='utf-8', newline='\n')

    def ask(self, text, prefix=''):
        return answer(self.root, text, prefix)

    def test_terms(self):
        self.assertEqual(terms('Fade between the Scenes'), ['fade', 'between', 'scene'])
        self.assertEqual(terms('UiOptionsScreen bodies'), ['ui', 'option', 'screen', 'body'])

    def test_exact_name_with_line_and_comment(self):
        text, found = self.ask('StepsIn')
        self.assertTrue(found)
        self.assertEqual(text, 'Core/Animation/Timeline.h:3 constexpr int StepsIn(double seconds, int stepsPerSecond)'
                               '  // Seconds as whole steps at a step rate.')

    def test_qualified_name_lists_every_overload_with_its_owner(self):
        text, _ = self.ask('CollisionWorld::Sweep')
        self.assertEqual(text.splitlines(), [
            'Core/Physics/CollisionWorld.h:9 int CollisionWorld::Sweep(int shape) const'
            '  // The nearest collider a moving shape reaches.',
            'Core/Physics/CollisionWorld.h:10 int CollisionWorld::Sweep(int shape, int mask) const',
        ])

    def test_type_prints_its_range_and_direct_members_once(self):
        lines = self.ask('CollisionWorld')[0].splitlines()
        self.assertEqual(lines[0], 'Core/Physics/CollisionWorld.h:3-16 class CollisionWorld  // Finds colliders fast.')
        self.assertEqual(lines[1:], ['  :5 CollisionWorld()', '  :7 void Move(int id)  // Moves a collider.',
                                     '  :9 int Sweep(int shape) const  // The nearest collider a moving shape reaches.',
                                     '  :10 int Sweep(int shape, int mask) const', '  :11-13 struct Hit'])
        self.assertNotIn('_count', '\n'.join(lines))

    def test_nested_type_and_case_folding(self):
        self.assertEqual(self.ask('CollisionWorld::Hit')[0].splitlines()[0],
                         'Core/Physics/CollisionWorld.h:11-13 struct CollisionWorld::Hit')
        self.assertIn('CollisionWorld::Move(int id)', self.ask('collisionworld::move')[0])

    def test_members_that_are_not_api_say_why_in_one_line(self):
        self.write('Core/Runtime/Game.h', GAME)
        self.assertEqual(self.ask('Game::RealSeconds()'), (
            'Core/Runtime/Game.h:9 float Game::RealSeconds() const  // protected: only Game and classes derived from '
            "it call it. This frame's real duration.", True))
        self.assertIn('// private: only Game itself uses it.', self.ask('Game::_frames')[0])
        self.assertIn('protected', self.ask('RealSeconds')[0])

    def test_enum_values_and_calls_are_names(self):
        self.write('Core/Runtime/Game.h', GAME)
        self.assertEqual(self.ask('InputKey::Space'),
                         ('Core/Runtime/Game.h:13 InputKey::Space  // A value of enum InputKey (lines 13-13)', True))
        self.assertTrue(self.ask('Game::Sounds()')[0].startswith('Core/Runtime/Game.h:6 int& Game::Sounds()'))
        self.assertTrue(self.ask('CollisionWorld::Sweep(int shape) const')[0].startswith(
            'Core/Physics/CollisionWorld.h:9 int CollisionWorld::Sweep(int shape) const'))

    def test_a_known_type_without_the_member_answers_briefly(self):
        self.write('Core/Runtime/Game.h', GAME)
        self.assertEqual(self.ask('Game::Sound'), (
            'Game (Core/Runtime/Game.h:3-12) has no public member Sound; closest:\n'
            "  :6 int& Sounds()  // The game's sound tables.", False))
        self.assertEqual(self.ask('Game::Zzz'), (
            'Game (Core/Runtime/Game.h:3-12) has no public member Zzz; ask python Engine api Game for its members.',
            False))

    def test_near_miss_names(self):
        text, found = self.ask('StepIn')
        self.assertTrue(found)
        self.assertEqual(text.splitlines()[0], 'Nothing is named StepIn; similar names:')
        self.assertIn('Timeline.h:3 constexpr int StepsIn', text)

    def test_words_give_routes_then_declarations(self):
        text, found = self.ask('fade between scenes')
        self.assertTrue(found)
        lines = text.splitlines()
        self.assertEqual(lines[:4], ['Tasks (files | recipe | concept docs):', '- Add a scene or fade between scenes',
                                     '  Core/Animation/Timeline.h | - | Docs/Runtime.md#scenes', 'Declarations:'])
        self.assertTrue(lines[4].startswith('Core/Animation/Timeline.h:5 void FadeScreen(float seconds)'))
        self.assertIn('transition', [w for task in TASKS['tasks'] for w in task.get('words', [])])
        self.assertIn('- Add a scene or fade between scenes', self.ask('scene transition')[0])

    def test_one_lower_case_word_is_a_topic(self):
        text, _ = self.ask('bullets')
        self.assertIn('- Stop fast bullets', text)

    def test_a_name_with_no_match_falls_back_to_words(self):
        text, found = self.ask('ScreenFader')
        self.assertTrue(found)
        self.assertEqual(text.splitlines()[0], 'Nothing is named ScreenFader; closest by words:')
        self.assertIn('FadeScreen', text)

    def test_nothing_matches(self):
        self.assertEqual(self.ask('xyzzy plugh'), ('Nothing matches "xyzzy plugh"; try other words or a type name.', False))

    def test_answers_are_cut_with_a_hint(self):
        members = ''.join(f'    void Member{n}();\n' for n in range(60))
        self.write('Core/Big.h', f'class Big {{\npublic:\n{members}}};\n')
        lines = self.ask('Big')[0].splitlines()
        self.assertEqual(len(lines), query.LIMIT)
        self.assertEqual(lines[-1], '(22 more lines; ask Owner::Name for one member, or read the printed line range)')

    def test_paths_open_from_the_project(self):
        self.assertTrue(self.ask('StepsIn', 'Engine/')[0].startswith('Engine/Core/Animation/Timeline.h:3 '))
        self.assertIn('  Engine/Core/Animation/Timeline.h | - | Engine/Docs/Runtime.md#scenes',
                      self.ask('fade between scenes', 'Engine/')[0])
        engine = self.root / 'Engine'
        with patch('Tools.commands.api.ENGINE_ROOT', engine):
            self.assertEqual(display_prefix(self.root), 'Engine/')
            self.assertEqual(display_prefix(engine), '')
            self.assertEqual(display_prefix(self.root / 'Elsewhere'), engine.as_posix() + '/')

    def test_command_exit_codes(self):
        with patch('Tools.commands.api.ENGINE_ROOT', self.root):
            for words, code in ((['StepsIn'], 0), (['fade', 'between', 'scenes'], 0), (['xyzzy'], 1)):
                output = io.StringIO()
                with contextlib.redirect_stdout(output):
                    self.assertEqual(cli.main(['--project', str(self.root), 'api', *words]), code)
                self.assertTrue(output.getvalue().strip())

    def test_invalid_tasks_table_is_a_tool_error(self):
        self.write('AgentDocs/Tasks.json', '{"tasks": [{"task": "x"}]}')
        with patch('Tools.commands.api.ENGINE_ROOT', self.root), contextlib.redirect_stderr(io.StringIO()) as error:
            self.assertEqual(cli.main(['--project', str(self.root), 'api', 'fade']), 2)
        self.assertIn('Tasks.json is invalid', error.getvalue())


class RealEngineTests(unittest.TestCase):
    """The shipped headers and Tasks.json answer the questions agents ask, within the limit."""

    def test_known_answers(self):
        root = Path(__file__).resolve().parents[2]
        text, found = answer(root, 'StepsIn')
        self.assertTrue(found and 'Core/Animation/Timeline.h:' in text)
        text, _ = answer(root, 'rain')
        self.assertIn('Core/Weather/Weather.h', text)
        for words in ('pause menu', 'hit feedback', 'options screen', 'screen shake', 'Painter'):
            text, found = answer(root, words)
            self.assertTrue(found, words)
            self.assertLessEqual(len(text.splitlines()), query.LIMIT, words)


if __name__ == '__main__':
    unittest.main()

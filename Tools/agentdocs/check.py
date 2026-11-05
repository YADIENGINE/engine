"""Check/regenerate API pages, copy marked C++ recipe regions into recipes and skills, and validate skills.

A recipe must show at least one region; a skill may show some. JSON examples in recipes and skills must parse.

Skills (`AgentDocs/Skills/<name>/SKILL.md`) need Agent Skills front matter whose name is the folder name, and a row in
`AgentDocs/Agent.md`. Every `python Engine <command> [<action>]` written in AgentDocs must exist in the tools' parser,
so a renamed command cannot leave an agent instruction behind. Every file and heading AgentDocs points at must exist
(links.py).
"""

import argparse
import json
import re
from pathlib import Path

from .api import generate
from .links import check_links

REGION = re.compile(r'^// \[recipe:([\w-]+)\]\n(.*?)^// \[/recipe\]\n?', re.M | re.S)
SNIPPET = re.compile(r'<!-- recipe:([\w-]+) -->\n```cpp\n(.*?)```\n<!-- /recipe -->', re.S)
JSON_BLOCK = re.compile(r'```json\n(.*?)```', re.S)
FRONT_MATTER = re.compile(r'\A---\nname: ([^\n]*)\ndescription: ([^\n]*)\n(?:[^\n]*\n)*?---\n')
TOOL_CALL = re.compile(r'python Engine ([a-z][\w-]*)(?: ([a-z][\w-]*))?')
DESCRIPTION_LIMIT = 1024  # Agent Skills format


def subcommands(parser: argparse.ArgumentParser) -> dict[str, argparse.ArgumentParser]:
    for action in parser._actions:  # argparse exposes subparsers only through its actions
        if isinstance(action, argparse._SubParsersAction):
            return dict(action.choices)
    return {}


def check_skills(root: Path, parser: argparse.ArgumentParser) -> list[str]:
    problems = []
    docs = root / 'AgentDocs'
    agent = docs / 'Agent.md'
    index = agent.read_text(encoding='utf-8') if agent.is_file() else ''
    for folder in sorted(path for path in (docs / 'Skills').glob('*') if path.is_dir()):
        where = f'AgentDocs/Skills/{folder.name}/SKILL.md'
        skill = folder / 'SKILL.md'
        if not skill.is_file():
            problems.append(f'{where}: missing')
            continue
        match = FRONT_MATTER.match(skill.read_text(encoding='utf-8'))
        if not match:
            problems.append(f'{where}: front matter must start with name and description lines')
        elif match[1].strip() != folder.name:
            problems.append(f'{where}: name "{match[1].strip()}" differs from its folder')
        elif not 0 < len(match[2].strip()) <= DESCRIPTION_LIMIT:
            problems.append(f'{where}: description must have 1-{DESCRIPTION_LIMIT} characters')
        if where not in index:
            problems.append(f'AgentDocs/Agent.md: no row for {where}')
    commands = subcommands(parser)
    for path in sorted(docs.rglob('*.md')):
        if path.parent.name == 'Api':
            continue
        for number, line in enumerate(path.read_text(encoding='utf-8').splitlines(), 1):
            for call in TOOL_CALL.finditer(line):
                where = f'{path.relative_to(root).as_posix()}:{number}'
                command = commands.get(call[1])
                if command is None:
                    problems.append(f'{where}: unknown command "python Engine {call[1]}"')
                elif (actions := subcommands(command)) and call[2] and call[2] not in actions:
                    problems.append(f'{where}: unknown action "python Engine {call[1]} {call[2]}"')
    return problems


def check_agentdocs(root: Path, fix: bool = False) -> tuple[list[str], list[str]]:
    problems, fixed = [], []
    output, _ = generate(root)
    api = root / 'AgentDocs/Api'

    def sync(path: Path, expected: str):
        if path.is_file() and path.read_text(encoding='utf-8') == expected:
            return
        where = path.relative_to(root).as_posix()
        if fix:
            path.parent.mkdir(parents=True, exist_ok=True)
            path.write_text(expected, encoding='utf-8', newline='\n')
            fixed.append(where)
        else:
            problems.append(f'{where}: stale or missing (fix: check --fix)')

    for name, content in output.items():
        sync(api / name, content)
    for path in sorted(api.glob('*.md')):
        if path.name not in output:
            if fix:
                path.unlink()
                fixed.append(path.relative_to(root).as_posix())
            else:
                problems.append(f'{path.relative_to(root).as_posix()}: obsolete generated page (fix: check --fix)')
    recipes = root / 'AgentDocs/Recipes'
    regions = {}
    for path in sorted((recipes / 'Code').glob('*')):
        if path.suffix not in ('.cpp', '.h'):
            continue
        text = path.read_text(encoding='utf-8')
        matches = list(REGION.finditer(text))
        if text.count('// [recipe:') != len(matches) or text.count('// [/recipe]') != len(matches):
            problems.append(f'{path.relative_to(root).as_posix()}: malformed recipe region')
        for match in matches:
            if match[1] in regions:
                problems.append(f'{path.relative_to(root).as_posix()}: duplicate region {match[1]}')
            regions[match[1]] = match[2]
    used = set()
    skills = sorted((root / 'AgentDocs/Skills').glob('*/SKILL.md'))
    for path in sorted(recipes.glob('*.md')) + skills:
        text = path.read_text(encoding='utf-8')
        where = path.relative_to(root).as_posix()

        def replace(match):
            name = match[1]
            if name in used:
                problems.append(f'{where}: duplicate snippet {name}')
            used.add(name)
            if name not in regions:
                problems.append(f'{where}: missing code region {name}')
                return match[0]
            return f'<!-- recipe:{name} -->\n```cpp\n{regions[name]}```\n<!-- /recipe -->'

        expected = SNIPPET.sub(replace, text)
        if text.count('<!-- recipe:') != len(SNIPPET.findall(text)):
            problems.append(f'{where}: malformed snippet marker')
        if path.name != 'SKILL.md' and not SNIPPET.search(text):
            problems.append(f'{where}: missing recipe snippet')
        for block in JSON_BLOCK.finditer(text):
            try:
                json.loads(block[1])
            except json.JSONDecodeError as error:
                problems.append(f'{where}: invalid JSON example ({error.msg}, line {error.lineno})')
        sync(path, expected)
    for name in sorted(regions.keys() - used):
        problems.append(f'AgentDocs/Recipes: code region {name} has no Markdown snippet')
    from ..cli import build_parser  # The parser module imports this checker
    problems += check_skills(root, build_parser())
    problems += check_links(root)
    return problems, fixed

"""Check/regenerate API pages and copy marked C++ recipe regions into Markdown."""

import re
from pathlib import Path

from .api import generate

REGION = re.compile(r'^// \[recipe:([\w-]+)\]\n(.*?)^// \[/recipe\]\n?', re.M | re.S)
SNIPPET = re.compile(r'<!-- recipe:([\w-]+) -->\n```cpp\n(.*?)```\n<!-- /recipe -->', re.S)


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
    for path in sorted(recipes.glob('*.md')):
        text = path.read_text(encoding='utf-8')

        def replace(match):
            name = match[1]
            if name in used:
                problems.append(f'{path.name}: duplicate snippet {name}')
            used.add(name)
            if name not in regions:
                problems.append(f'{path.name}: missing code region {name}')
                return match[0]
            return f'<!-- recipe:{name} -->\n```cpp\n{regions[name]}```\n<!-- /recipe -->'

        expected = SNIPPET.sub(replace, text)
        if text.count('<!-- recipe:') != len(SNIPPET.findall(text)):
            problems.append(f'{path.name}: malformed snippet marker')
        if not SNIPPET.search(text):
            problems.append(f'{path.name}: missing recipe snippet')
        sync(path, expected)
    for name in sorted(regions.keys() - used):
        problems.append(f'AgentDocs/Recipes: code region {name} has no Markdown snippet')
    return problems, fixed

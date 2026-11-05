"""API names in backticks in AgentDocs prose must exist, so that a renamed type or function cannot leave an agent
instruction behind.

Checked outside code blocks, in `Agent.md`, skills and recipes: names of two or more CamelCase words (`StepsIn`,
`UiOptionsScreen`) and both parts of `Type::member`. A name passes when a public header or recipe code declares or uses
it. Paths, commands and quoted text inside backticks are not names. Lower-case `a.b` forms (`settings.buttons`) are not
checked: the first part is a local name.
"""

import re
from pathlib import Path

from .api import TOKEN, public_headers
from .links import _prose_lines

# Placeholders that stand for a game's own names
ALLOWED = {'Bass', 'DrawX', 'Drums', 'MenuPart', 'MyUiTable', 'WxH'}
SPAN = re.compile(r'`([^`\n]+)`')
CAMEL = re.compile(r'(?<![\w.:])[A-Z][a-z0-9]+[A-Z]\w*')
SCOPED = re.compile(r'(?<![\w:])([A-Z]\w*)::(~?[A-Za-z_]\w*)')
QUOTED = re.compile(r'"(?:\\.|[^"\\])*"|\'(?:\\.|[^\'\\])*\'')


def known_identifiers(root: Path) -> set[str]:
    """Identifiers in public headers and recipe code, comments and strings left out."""
    sources = public_headers(root) + sorted((root / 'AgentDocs/Recipes/Code').glob('*.[ch]*'))
    found = set()
    for path in sources:
        for token in TOKEN.findall(path.read_text(encoding='utf-8')):
            if re.fullmatch(r'[A-Za-z_]\w*', token):
                found.add(token)
    return found


def _names(span: str) -> list[str]:
    if '/' in span or span.startswith('python ') or re.search(r'\.[a-z]{1,4}\b', span):
        return []  # A path, a command or a file name
    span = QUOTED.sub('', span)
    names = [name for pair in SCOPED.findall(span) for name in pair]
    return names + [name for name in CAMEL.findall(SCOPED.sub(' ', span))]


def check_names(root: Path) -> list[str]:
    docs = root / 'AgentDocs'
    files = [docs / 'Agent.md'] + sorted(docs.glob('Skills/*/SKILL.md')) + sorted(docs.glob('Recipes/*.md'))
    files = [path for path in files if path.is_file()]
    if not files:
        return []
    known = known_identifiers(root) | ALLOWED
    problems = []
    for path in files:
        where = path.relative_to(root).as_posix()
        for number, line in _prose_lines(path.read_text(encoding='utf-8')):
            for span in SPAN.findall(line):
                for name in _names(span):
                    if name.lstrip('~') not in known:
                        problems.append(f'{where}:{number}: `{name}` is declared in no public header or recipe code')
    return problems

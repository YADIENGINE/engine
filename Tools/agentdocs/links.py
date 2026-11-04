"""Every file and heading AgentDocs points at must exist, so that an agent following a route never reads a dead end.

Two kinds of reference are checked, outside code blocks:
- Markdown links, `[text](path#heading)`, relative to the file they are in.
- Paths in backticks that name an engine file: `Docs/Ui.md#sounds`, `Core/Ui/Kernel/Ui.h`, `Recipes/UiMenu.md`.
  `Recipes/` and `Skills/` are under AgentDocs; `Engine/` in front is the engine's folder in a project. A path with a
  placeholder (`<name>`, `*`) or any other first folder, such as a project's own `Game/` or `Editor/`, is not checked.

A `#heading` must match a heading of the target, with GitHub's rule for making a heading into an anchor. Generated API
pages are not scanned: they are made from the headers themselves.
"""

import re
from pathlib import Path

MARKDOWN_LINK = re.compile(r'(?<!!)\[[^\]\n]*\]\(([^)\s]+)\)')
BACKTICK = re.compile(r'`([^`\s]+)`')
ENGINE_FOLDERS = ('Docs/', 'AgentDocs/', 'Assets/', 'Core/', 'Tools/')
AGENTDOCS_FOLDERS = ('Recipes/', 'Skills/')
HEADING = re.compile(r'^#{1,6}\s+(.*?)\s*#*\s*$')
FENCE = re.compile(r'^\s*(```|~~~)')
PLACEHOLDER = re.compile(r'[<>*{}]|\.\.\.')


def anchor(heading: str) -> str:
    """GitHub's anchor for a heading: lower case, punctuation dropped, spaces to hyphens."""
    text = re.sub(r'`|\*\*|__', '', heading.strip()).lower()
    text = re.sub(r'\[([^\]]*)\]\([^)]*\)', r'\1', text)
    text = re.sub(r'[^\w\- ]', '', text)
    return text.replace(' ', '-')


def anchors(text: str) -> set[str]:
    found, seen = set(), {}
    fenced = False
    for line in text.splitlines():
        if FENCE.match(line):
            fenced = not fenced
            continue
        match = None if fenced else HEADING.match(line)
        if not match:
            continue
        base = anchor(match[1])
        count = seen.get(base, 0)
        seen[base] = count + 1
        found.add(base if count == 0 else f'{base}-{count}')
    return found


def _prose_lines(text: str):
    fenced = False
    for number, line in enumerate(text.splitlines(), 1):
        if FENCE.match(line):
            fenced = not fenced
        elif not fenced:
            yield number, line


def _backtick_target(root: Path, reference: str) -> Path | None:
    if PLACEHOLDER.search(reference) or '://' in reference:
        return None
    path = reference.split('#', 1)[0].rstrip('.,;:')
    if path.startswith('Engine/'):
        path = path[len('Engine/'):]
    if path.startswith(AGENTDOCS_FOLDERS):
        return root / 'AgentDocs' / path
    if path.startswith(ENGINE_FOLDERS) and Path(path).suffix:
        return root / path
    return None


def check_links(root: Path) -> list[str]:
    problems = []
    cache: dict[Path, set[str]] = {}
    docs = root / 'AgentDocs'
    for source in sorted(docs.rglob('*.md')):
        if source.parent == docs / 'Api':
            continue
        where_file = source.relative_to(root).as_posix()
        for number, line in _prose_lines(source.read_text(encoding='utf-8')):
            references = []
            for match in MARKDOWN_LINK.finditer(line):
                target = match[1]
                if '://' in target or target.startswith('mailto:'):
                    continue
                path, _, heading = target.partition('#')
                references.append((target, source if not path else source.parent / path, heading))
            for match in BACKTICK.finditer(line):
                if (target := _backtick_target(root, match[1])) is not None:
                    references.append((match[1], target, match[1].partition('#')[2].rstrip('.,;:')))
            for text, target, heading in references:
                where = f'{where_file}:{number}'
                if not target.exists():
                    problems.append(f'{where}: "{text}" points at a file that does not exist')
                elif heading and target.suffix == '.md':
                    resolved = target.resolve()
                    if resolved not in cache:
                        cache[resolved] = anchors(target.read_text(encoding='utf-8'))
                    if heading not in cache[resolved]:
                        problems.append(f'{where}: "{text}" points at a heading that does not exist')
    return problems

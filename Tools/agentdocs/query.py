"""Answer `python Engine api QUERY` from the public headers and `AgentDocs/Tasks.json`, read live on every call.

A name (`StepsIn`, `CollisionWorld`, `CollisionWorld::Sweep`, also written as a call: `Game::Sounds()`) is looked up
in this order, stopping at the first level that matches: the exact public name (with `Owner::` when given; every
overload), the same ignoring case; a protected or private member, said to be one in a single line; an enum value
(`InputKey::Space`); for `Owner::Name` with a known Owner, the type's closest members; near misses (`StepIn`). A type
prints its line range and its direct members. A miss costs a few lines, never a words list for a mistyped member.
Anything else, a single lower-case word ("rain"), or a name that still matches nothing is a words query: the best task
routes of `Tasks.json`, then the declarations whose names, owners, comments and paths share the most words with it.

The answer is at most LIMIT lines, one declaration per line as `path:line signature  // first comment sentence`.
Paths start at the project root (`Engine/Core/...` in a game), so an agent can open the printed line range directly.
"""

import difflib
import re
from dataclasses import dataclass
from pathlib import Path

from .api import Declaration, Header, parse_public
from .tasks import Task, load_tasks

LIMIT = 40
TASK_ROUTES = 3
WORD_DECLARATIONS = 15  # A words query points the way; a name query then gives the full answer
CLOSE_NAMES = 4
NAME = re.compile(r'(?:~?[A-Za-z_]\w*::)*(?:~?[A-Za-z_]\w*|operator\S+)')
WORD = re.compile(r'[A-Z]+(?=[A-Z][a-z])|[A-Z]?[a-z]+|[A-Z]+|\d+')
STOP = {'a', 'an', 'and', 'are', 'as', 'at', 'be', 'by', 'can', 'do', 'does', 'for', 'from', 'get', 'how', 'i', 'in',
        'into', 'is', 'it', 'its', 'make', 'my', 'of', 'on', 'one', 'or', 'own', 'set', 'that', 'the', 'their', 'this',
        'to', 'use', 'using', 'want', 'what', 'when', 'where', 'which', 'with', 'without'}
# Weight of a word found in each part of a task or declaration
TASK_WEIGHTS = {'task': 3, 'words': 3, 'files': 2}
DECLARATION_WEIGHTS = {'name': 4, 'owner': 2, 'comment': 1, 'path': 1}
ROUTE_BOOST = 2  # A declaration in a file of a printed task route


@dataclass
class Found:
    header: Header
    declaration: Declaration


def stem(word: str) -> str:
    word = word.lower()
    if len(word) > 4 and word.endswith('ies'):
        return word[:-3] + 'y'
    if len(word) > 3 and word.endswith('s') and not word.endswith(('ss', 'us', 'is')):
        return word[:-1]
    return word


def terms(text: str) -> list[str]:
    """Words of a text, CamelCase split, lower case, plural `s` dropped, common words left out; first order kept."""
    found = []
    for word in WORD.findall(text):
        word = stem(word)
        if word not in STOP and word not in found:
            found.append(word)
    return found


def _owner_matches(owner: str, wanted: str, fold: bool) -> bool:
    if fold:
        owner, wanted = owner.lower(), wanted.lower()
    return not wanted or owner == wanted or owner.endswith('::' + wanted)


def _full_name(declaration: Declaration) -> str:
    return '::'.join(part for part in (declaration.owner, declaration.name) if part)


def find_name(found: list[Found], query: str) -> list[Found]:
    *owner, name = query.split('::')
    wanted = '::'.join(owner)
    for fold in (False, True):
        matches = [item for item in found
                   if (item.declaration.name.lower() == name.lower() if fold else item.declaration.name == name)
                   and _owner_matches(item.declaration.owner, wanted, fold)]
        if matches:
            # A type lists its constructors among its members; do not print them twice.
            types = {_full_name(item.declaration) for item in matches if item.declaration.is_type}
            return [item for item in matches if item.declaration.is_type or item.declaration.owner not in types]
    return []


def qualified(declaration: Declaration) -> str:
    """The signature with the owner in front of the declared name, as C++ writes it outside the class:
    `void CollisionWorld::Move(...)`, `struct CollisionWorld::Hit`."""
    if not declaration.owner:
        return declaration.signature
    pattern = rf'(?<![\w:~]){re.escape(declaration.name)}(?!\w)'
    text, count = re.subn(pattern, lambda match: f'{declaration.owner}::{match[0]}', declaration.signature, count=1)
    return text if count else f'{declaration.owner}: {declaration.signature}'


def _close_names(found: list[Found], query: str) -> list[Found]:
    """Declarations whose name is a near miss of the query's last part (`StepIn` for `StepsIn`)."""
    *owner, name = query.split('::')
    wanted = '::'.join(owner)
    candidates = [item for item in found if _owner_matches(item.declaration.owner, wanted, fold=True)]
    by_lower: dict[str, list[Found]] = {}
    for item in candidates:
        by_lower.setdefault(item.declaration.name.lower(), []).append(item)
    close = difflib.get_close_matches(name.lower(), list(by_lower), n=CLOSE_NAMES, cutoff=0.85)
    return [item for lower in close for item in by_lower[lower]][:CLOSE_NAMES * 2]


class Answer:
    """Lines of the answer, cut at LIMIT with a note on how to see the rest."""

    def __init__(self, prefix: str):
        self.prefix = prefix
        self.lines: list[str] = []

    def path(self, path: str) -> str:
        return self.prefix + path

    def declaration(self, header: Header, declaration: Declaration, indent: bool = False):
        where = f':{declaration.line}' if indent else f'{self.path(header.path)}:{declaration.line}'
        if declaration.is_type:
            where += f'-{declaration.end}'
        signature = declaration.signature if indent else qualified(declaration)
        comment = f'  // {declaration.comment}' if declaration.comment else ''
        self.lines.append(('  ' if indent else '') + f'{where} {signature}{comment}')

    def text(self, hint: str) -> str:
        if len(self.lines) <= LIMIT:
            return '\n'.join(self.lines)
        shown = self.lines[:LIMIT - 1]
        return '\n'.join(shown + [f'({len(self.lines) - len(shown)} more lines; {hint})'])


def _hidden_answer(answer: Answer, matches: list[Found]):
    """Why a member that exists is not API: one line each, with where it is and what it is for."""
    for item in matches:
        declaration = item.declaration
        who = (f'only {declaration.owner} and classes derived from it call it' if declaration.access == 'protected'
               else f'only {declaration.owner} itself uses it')
        comment = f' {declaration.comment}' if declaration.comment else ''
        answer.lines.append(f'{answer.path(item.header.path)}:{declaration.line} {qualified(declaration)}'
                            f'  // {declaration.access}: {who}.{comment}')


def _enum_values(found: list[Found], query: str) -> list[tuple[Found, str, int]]:
    """Enumerators named like the query (`InputKey::Space`, or `Space` alone): the enum, the value and its line."""
    *owner, name = query.split('::')
    wanted = '::'.join(owner)
    for fold in (False, True):
        hits = [(item, value, line) for item in found if item.declaration.values
                and _owner_matches(_full_name(item.declaration), wanted, fold)
                for value, line in item.declaration.values if (value.lower() == name.lower() if fold else value == name)]
        if hits:
            return hits
    return []


def _missing_member(answer: Answer, found: list[Found], query: str) -> bool:
    """`Owner::Name` where Owner is a known type but has no such member: the type and its closest members, briefly,
    instead of a words search. False when Owner is not a type."""
    *owner, name = query.split('::')
    wanted = '::'.join(owner)
    types = [item for item in found if item.declaration.is_type and wanted
             and _owner_matches(_full_name(item.declaration), wanted, fold=True)]
    if not types:
        return False
    for item in types:
        full = _full_name(item.declaration)
        members = {member.name: member for member in item.header.declarations if member.owner == full}
        close = difflib.get_close_matches(name, list(members), n=CLOSE_NAMES, cutoff=0.5)
        where = f'{answer.path(item.header.path)}:{item.declaration.line}-{item.declaration.end}'
        answer.lines.append(f'{full} ({where}) has no public member {name}'
                            + ('; closest:' if close else f'; ask python Engine api {full} for its members.'))
        for member in close:
            answer.declaration(item.header, members[member], indent=True)
    return True


def _name_answer(answer: Answer, matches: list[Found]):
    for item in matches:
        answer.declaration(item.header, item.declaration)
        if not item.declaration.is_type:
            continue
        full = _full_name(item.declaration)
        for member in item.header.declarations:
            if member.owner == full:
                answer.declaration(item.header, member, indent=True)


def _score(words: list[str], fields: dict[str, set[str]], weights: dict[str, int]) -> tuple[int, int]:
    """(words matched, weighted score); a word counts once, in its heaviest field."""
    matched = score = 0
    for word in words:
        weight = max((weights[name] for name, values in fields.items() if word in values), default=0)
        matched += weight > 0
        score += weight
    return matched, score


def _best(scored: list[tuple[tuple[int, int], object]], lenient: bool) -> list[object]:
    """Items that match the most query words (`lenient`: or one fewer), most words and heaviest first; nothing when
    no word matched."""
    best = max((score[0] for score, _ in scored), default=0)
    if not best:
        return []
    least = max(1, best - 1) if lenient else best
    ranked = sorted((item for item in scored if item[0][0] >= least), key=lambda item: (-item[0][0], -item[0][1]))
    return [item for _, item in ranked]


def _task_fields(task: Task) -> dict[str, set[str]]:
    return {'task': set(terms(task.task)), 'words': {stem(word) for word in task.words},
            'files': {word for file in task.files for word in terms(Path(file).stem)}}


def _declaration_fields(item: Found) -> dict[str, set[str]]:
    declaration = item.declaration
    return {'name': set(terms(declaration.name)), 'owner': set(terms(declaration.owner)),
            'comment': set(terms(declaration.comment)), 'path': set(terms(item.header.path.rsplit('.', 1)[0]))}


def _words_answer(answer: Answer, words: list[str], found: list[Found], tasks: list[Task]) -> bool:
    # A route is advice to open files: only the best covered tasks. Declarations are cheap to skim: one word fewer.
    routes = _best([(_score(words, _task_fields(task), TASK_WEIGHTS), task) for task in tasks], False)[:TASK_ROUTES]
    route_files = {file for task in routes for file in task.files}
    scored = []
    for item in found:
        fields = _declaration_fields(item)
        # Members of `struct ColliderHit` all share its owner; only a declaration named or described by a query word
        # is about it.
        if not any(word in fields['name'] or word in fields['comment'] for word in words):
            continue
        matched, score = _score(words, fields, DECLARATION_WEIGHTS)
        if item.header.path in route_files:
            score += ROUTE_BOOST
        scored.append(((matched, score), item))
    declarations = _best(scored, len(words) >= 2)[:WORD_DECLARATIONS]
    if routes:
        answer.lines.append('Tasks (files | recipe | concept docs):')
        for task in routes:
            route = [', '.join(answer.path(file) for file in task.files)]
            route += [answer.path(task.recipe) if task.recipe else '-', ', '.join(answer.path(d) for d in task.docs)]
            answer.lines += [f'- {task.task}', '  ' + ' | '.join(route)]
    if declarations:
        answer.lines.append('Declarations:')
        for item in declarations:
            answer.declaration(item.header, item.declaration)
    return bool(routes or declarations)


def answer(root: Path, query: str, prefix: str = '') -> tuple[str, bool]:
    """The answer to `query` for the engine at `root`, with `prefix` before every path; and whether anything matched."""
    query = ' '.join(query.split())
    # A call as written in code names its function: `Game::Sounds()`, `Painter::Text(...) const`.
    query = re.sub(r'(?<=\w)\s*\(.*\)\s*(?:const)?\s*;?$', '', query) or query
    headers = parse_public(root)
    found = [Found(header, declaration) for header in headers for declaration in header.declarations]
    result = Answer(prefix)
    # One lower-case word ("rain") is a topic more often than a field name: the words search shows both.
    name = NAME.fullmatch(query) and not re.fullmatch(r'[a-z]+', query)
    if name and (matches := find_name(found, query)):
        _name_answer(result, matches)
        return result.text('ask Owner::Name for one member, or read the printed line range'), True
    if name and (hidden := find_name([Found(h, d) for h in headers for d in h.hidden], query)):
        _hidden_answer(result, hidden)
        return result.text('ask one of them by Owner::Name'), True
    if name and (values := _enum_values(found, query)):
        for item, value, line in values:
            declaration = item.declaration
            result.lines.append(f'{result.path(item.header.path)}:{line} {_full_name(declaration)}::{value}'
                                f'  // A value of enum {_full_name(declaration)} (lines {declaration.line}-'
                                f'{declaration.end})')
        return result.text('ask Enum::Value'), True
    if name and '::' in query and _missing_member(result, found, query):
        return result.text('ask the type for its members'), False
    if name and (close := _close_names(found, query)):
        result.lines.append(f'Nothing is named {query}; similar names:')
        for item in close:
            result.declaration(item.header, item.declaration)
        return result.text('ask one of the names'), True
    words = terms(query.replace('::', ' '))
    if name:
        result.lines.append(f'Nothing is named {query}; closest by words:')
    if not words or not _words_answer(result, words, found, load_tasks(root)):
        return f'Nothing matches "{query}"; try other words or a type name.', False
    return result.text('add a word to narrow the query, or ask a name'), True

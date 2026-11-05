"""`AgentDocs/Tasks.json` routes a game-development task to its files, compiled recipe and concept docs.

Agents do not read the file; `python Engine api WORDS` searches it. Paths start at the engine root. `words` lists
words a developer may use that the task text lacks ("pause" for an options screen). `check` keeps every route alive:
files exist and reach released copies, headers are public, the recipe is an AgentDocs recipe, and each doc heading
exists.
"""

import json
import re
from dataclasses import dataclass, field
from pathlib import Path

from .api import public_headers
from .links import Targets

TASKS = 'AgentDocs/Tasks.json'
FIELDS = ('task', 'files', 'recipe', 'docs', 'words')


@dataclass
class Task:
    task: str
    files: list[str]
    recipe: str | None
    docs: list[str]
    words: list[str] = field(default_factory=list)


def load_tasks(root: Path) -> list[Task]:
    """The tasks of the engine at `root`; an empty list when there is no table. Raises ValueError when it is invalid."""
    path = root / TASKS
    if not path.is_file():
        return []
    data = json.loads(path.read_text(encoding='utf-8'))
    if not isinstance(data, dict) or not isinstance(data.get('tasks'), list):
        raise ValueError(f'{TASKS}: expected {{"tasks": [...]}}')
    return [Task(item['task'], item['files'], item.get('recipe'), item['docs'], item.get('words', []))
            for item in data['tasks']]


def _strings(value, allow_empty: bool = False) -> bool:
    return (isinstance(value, list) and (allow_empty or bool(value))
            and all(isinstance(item, str) and item.strip() for item in value))


def check_tasks(root: Path) -> list[str]:
    path = root / TASKS
    if not path.is_file():
        return [f'{TASKS}: missing']
    try:
        data = json.loads(path.read_text(encoding='utf-8'))
    except json.JSONDecodeError as error:
        return [f'{TASKS}: invalid JSON ({error.msg}, line {error.lineno})']
    if not isinstance(data, dict) or set(data) != {'tasks'} or not isinstance(data['tasks'], list):
        return [f'{TASKS}: expected an object with one "tasks" list']
    problems = []
    targets = Targets(root)
    public = {header.relative_to(root).as_posix() for header in public_headers(root)}
    seen = set()
    for number, item in enumerate(data['tasks'], 1):
        name = item.get('task') if isinstance(item, dict) else None
        where = f'{TASKS}: task {number}' + (f' "{name}"' if isinstance(name, str) else '')
        if not isinstance(item, dict):
            problems.append(f'{where}: expected an object')
            continue
        if unknown := sorted(set(item) - set(FIELDS)):
            problems.append(f'{where}: unknown field(s) {", ".join(unknown)}')
        if not isinstance(name, str) or not name.strip():
            problems.append(f'{where}: "task" must be a non-empty text')
        elif name in seen:
            problems.append(f'{where}: duplicate task')
        seen.add(name)
        if not _strings(item.get('files')):
            problems.append(f'{where}: "files" must be a non-empty list of paths')
        if not _strings(item.get('docs')):
            problems.append(f'{where}: "docs" must be a non-empty list of paths')
        recipe = item.get('recipe')
        if recipe is not None and not (isinstance(recipe, str) and re.fullmatch(r'AgentDocs/Recipes/\w+\.md', recipe)):
            problems.append(f'{where}: "recipe" must be null or AgentDocs/Recipes/<Name>.md')
            recipe = None
        if 'words' in item and not (_strings(item['words']) and all(w == w.lower() for w in item['words'])):
            problems.append(f'{where}: "words" must be a list of lower-case words')
        files = item['files'] if _strings(item.get('files')) else []
        docs = item['docs'] if _strings(item.get('docs')) else []
        for reference in files + ([recipe] if recipe else []) + docs:
            file, _, heading = reference.partition('#')
            if problem := targets.problem(reference, root / file, heading):
                problems.append(f'{where}: {problem}')
            elif file.endswith('.h') and file not in public:
                problems.append(f'{where}: "{reference}" is not a public header')
    return problems

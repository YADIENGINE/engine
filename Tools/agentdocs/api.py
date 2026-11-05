"""Heuristic C++ declaration extraction, without preprocessing or evaluating C++.

`python Engine api` reads public declarations through this parser on every call, so there is no stored index to go
stale; `query.py` searches the result.
"""

import bisect
import re
from dataclasses import dataclass, field
from pathlib import Path

TOKEN = re.compile(r'//[^\n]*|/\*[\s\S]*?\*/|"(?:\\.|[^"\\])*"|\'(?:\\.|[^\'\\])*\'|'
                   r'[A-Za-z_]\w*|\d+(?:\.\d+)?[\w]*|::|&&|->|\.\.\.|==|!=|<=|>=|\+=|-=|\*=|/=|\|\||[^\s]')
# Keep agent-facing material English without editing legacy header comments. Remove an entry when translated upstream.
UNTRANSLATED = {('Core/Tuning/TuningLive.h', 'TuningReport', 'std::string source')}
TYPE = re.compile(r'(?:class|struct|union|enum(?:\s+class|\s+struct)?)\s+(?:\[\[.*?\]\]\s*)?(\w+)')
OPERATOR = re.compile(r'\boperator\s*(\(\)|[^\s(]+)\s*\(')
IDENTIFIER = re.compile(r'~?[A-Za-z_]\w*')


@dataclass
class Declaration:
    signature: str
    comment: str
    owner: str = ""
    name: str = ""  # The declared name: a function, type, alias, constant or field
    line: int = 0  # 1-based line of the declaration's first token
    end: int = 0  # Last line of a type's body; 0 for everything else
    access: str = "public"  # protected or private for a member of `Header.hidden`
    values: list[tuple[str, int]] = field(default_factory=list)  # An enum's values with their lines

    @property
    def is_type(self) -> bool:
        return self.end > 0


@dataclass
class Header:
    path: str
    summary: str = ""
    declarations: list[Declaration] = field(default_factory=list)
    skipped: int = 0
    # Protected and private members of public classes: not API, but `api` says why a name it was asked is missing
    hidden: list[Declaration] = field(default_factory=list)


def _outside_angles(text: str) -> str:
    """The text with template argument lists removed: `std::map<int, Hit> hits` gives `std::map hits`."""
    kept, depth = [], 0
    for char in text:
        if char == '<':
            depth += 1
        elif char == '>' and depth:
            depth -= 1
        elif not depth:
            kept.append(char)
    return ''.join(kept)


def declared_name(signature: str) -> str:
    """The name a signature declares: before `(` for a function, after the keyword for a type, the alias of `using`
    or `concept`, the last identifier before `=` or `{` for a constant or field."""
    text = re.sub(r'\[\[.*?\]\]', ' ', signature).strip()
    while re.match(r'template\s*<', text):
        depth, index = 0, len('template')
        for index in range(len('template'), len(text)):
            depth += {'<': 1, '>': -1}.get(text[index], 0)
            if depth == 0 and text[index] == '>':
                break
        text = text[index + 1:].strip()
    if alias := re.match(r'(?:using|concept)\s+(\w+)\s*=', text):
        return alias[1]
    if kind := TYPE.match(text):
        return kind[1]
    if operator := OPERATOR.search(text):
        # A conversion operator names a type: `operator bool`; the others are symbols: `operator==`, `operator[]`.
        return 'operator' + (' ' if re.match(r'\w', operator[1]) else '') + operator[1]
    outer = _outside_angles(text)
    for index, char in enumerate(outer):
        if char in '(={':
            names = IDENTIFIER.findall(re.sub(r'\[[^\]]*\]', '', outer[:index]))
            return names[-1] if names else ''
    names = IDENTIFIER.findall(re.sub(r'\[[^\]]*\]', '', outer))
    return names[-1] if names else ''


def sentence(comment: str) -> str:
    text = re.sub(r'\s+', ' ', comment).strip(' /*\t')
    protected = text.replace('e.g.', 'e\0g\0').replace('i.e.', 'i\0e\0')
    return re.split(r'(?<=[.!?])\s+', protected, maxsplit=1)[0].replace('\0', '.')


def compact(tokens: list[str]) -> str:
    text = ' '.join(tokens)
    text = re.sub(r'\s*(::|->)\s*', r'\1', text)
    text = re.sub(r'\s+([,;)>\]])', r'\1', text)
    text = re.sub(r'([(<\[])\s+', r'\1', text)
    text = re.sub(r'\s+([(&*])', r'\1', text)
    text = re.sub(r'(\w)\s+<', r'\1<', text)
    text = re.sub(r'\s*\.\s*', '.', text)
    text = re.sub(r'~\s+', '~', text)
    text = re.sub(r'operator\s+([=+*/<>!\-]+)', r'operator\1', text)
    text = text.replace('[ [', '[[').replace('] ]', ']]')
    return text.strip()


def navigation_noise(signature: str, owner: str, definition: bool) -> bool:
    if not definition and re.fullmatch(
            r'(?:template<.*>\s*)?(?:class|struct)\s+(?:\[\[.*?\]\]\s*)?\w+(?:<.*>)?', signature):
        return True
    if re.search(r'=\s*delete\b|~\s*\w+\s*\(', signature):
        return True
    if not owner:
        return False
    name = re.escape(owner.rsplit('::', 1)[-1])
    operation = re.search(rf'(?:\b{name}|operator=)\s*\(([^()]*)\)', signature)
    if not operation:
        return False
    # A copy constructor may have extra defaulted parameters. Ignore commas inside the self type's template args.
    parameters = re.split(r',(?![^<]*>)', operation[1])
    reference = r'&{0,2}' if operation[0].startswith('operator=') else r'&{1,2}'
    self_reference = rf'(?:const\s+)?{name}(?:<.*>)?\s*(?:const\s*)?{reference}\s*(?:\w+)?'
    return (re.fullmatch(self_reference, parameters[0].strip()) is not None
            and all('=' in parameter for parameter in parameters[1:]))


def file_comment(text: str, matches: list[re.Match]) -> tuple[str, int]:
    """A leading comment block separated from declarations by a blank line belongs to the file."""
    comments = []
    end = 0
    for match in matches:
        if not match[0].startswith(('//', '/*')) or (comments and text[end:match.start()].count('\n') > 1):
            break
        comments.append(match[0][2:] if match[0].startswith('//') else match[0][2:-2])
        end = match.end()
    else:
        return sentence(' '.join(comments)), end
    if comments and text[end:match.start()].count('\n') > 1:
        return sentence(' '.join(comments)), end
    return '', 0


def parse_header(path: str, text: str) -> Header:
    """Skip bodies, private/protected members and preprocessor directives; retain authored signatures.

    Conditional branches are indexed as written. Macros are not expanded; unrecognized public statements
    increment skipped. Aggregate fields are included because callers need designated initializers.
    """
    text = re.sub(r'^[ \t]*#(?:[^\n]*\\\n)*[^\n]*',
                  lambda match: '\n' * match[0].count('\n'), text, flags=re.M)
    matches = list(TOKEN.finditer(text))
    tokens = [match[0] for match in matches]
    result = Header(path)
    result.summary, file_comment_end = file_comment(text, matches)
    newlines = [index for index, char in enumerate(text) if char == '\n']

    def line_of(index: int) -> int:
        """Directives were blanked line for line, so a token's line is its line in the file."""
        return bisect.bisect_left(newlines, matches[index].start()) + 1

    def balanced(index: int, opening: str, closing: str) -> int:
        depth = 1
        while index < len(tokens) and depth:
            if tokens[index] == opening:
                depth += 1
            elif tokens[index] == closing:
                depth -= 1
            index += 1
        return index

    def emit(parts: list[str], comment: str, owner: str, seen: str | None, first: int,
             definition: bool = False) -> Declaration | None:
        """`seen`: the access of a statement whose enclosing scope is visible (public, protected, private); None
        when nothing encloses it visibly. Public statements become declarations, the others `hidden`."""
        if seen is None or not parts:
            return None
        visible = seen == 'public'
        signature = compact(parts).rstrip(';')
        # Large aggregate constants need a name/type, not hundreds of palette or style initializer tokens.
        if 'constexpr' in parts and '(' not in parts and '{' in parts and len(signature) > 180:
            signature = compact(parts[:parts.index('{')])
        if parts[0] in ('friend', 'static_assert'):
            return None
        if navigation_noise(signature, owner, definition):
            return None
        if (path, owner, signature) in UNTRANSLATED:
            comment = ''
        if len(parts) >= 2 and re.search(r'\w', signature):
            declaration = Declaration(signature, sentence(comment), owner, declared_name(signature), line_of(first),
                                      access=seen)
            (result.declarations if visible else result.hidden).append(declaration)
            return declaration
        if visible:
            result.skipped += 1
        return None

    def enum_values(begin: int, end: int) -> list[tuple[str, int]]:
        """Each enumerator's name and line: the first identifier of each comma-separated item."""
        values, depth, expecting = [], 0, True
        for index in range(begin, end):
            token = tokens[index]
            if token.startswith(('//', '/*')):
                continue
            if token in '([{<':
                depth += 1
            elif token in ')]}>':
                depth -= 1
            elif token == ',' and depth == 0:
                expecting = True
            elif expecting and depth == 0 and re.fullmatch(r'[A-Za-z_]\w*', token):
                values.append((token, line_of(index)))
                expecting = False
        return values

    def scope(index: int, owner: str = '', access: str = 'public', enclosing: bool = True) -> int:
        pending: list[str] = []
        comments: list[str] = []
        parens = brackets = first = 0  # first: the token that started the pending statement
        while index < len(tokens):
            token = tokens[index]
            index += 1
            if matches[index - 1].end() <= file_comment_end:
                continue
            if token.startswith(('//', '/*')):
                cleaned = token[2:] if token.startswith('//') else token[2:-2]
                cleaned = cleaned.strip().lstrip('*').strip()
                if cleaned.startswith(('clang-format', '---')):
                    continue
                prefix = text[text.rfind('\n', 0, matches[index - 1].start()) + 1:matches[index - 1].start()]
                if prefix.strip() and not pending:
                    if enclosing and access == 'public' and result.declarations and not result.declarations[-1].comment:
                        previous = result.declarations[-1]
                        if (path, previous.owner, previous.signature) not in UNTRANSLATED:
                            previous.comment = sentence(cleaned)
                    continue
                if not pending:
                    comments.append(cleaned)
                continue
            if token == '}' and not parens and not brackets:
                return index
            if (not pending and token in ('public', 'private', 'protected')
                    and index < len(tokens) and tokens[index] == ':'):
                access = token
                index += 1
                comments.clear()
                continue
            visible = enclosing and access == 'public'
            seen = access if enclosing else None
            if token == '(':
                parens += 1
            elif token == ')':
                parens -= 1
            elif token == '[':
                brackets += 1
            elif token == ']':
                brackets -= 1
            if token == ';' and parens == 0 and brackets == 0:
                emit(pending, ' '.join(comments), owner, seen, first)
                pending, comments = [], []
                continue
            if token == '{' and parens == 0 and brackets == 0:
                # Ignore template parameter class/struct keywords when finding the declaration kind.
                head = compact(pending)
                kind = re.search(r'(?:^|>\s*)(class|struct|enum(?: class)?)\s+(?:\[\[.*?\]\]\s*)?(\w+)', head)
                if kind and '(' not in head:
                    name = kind[2]
                    if kind[1].startswith('enum'):
                        end = balanced(index, '{', '}')
                        values = [t for t in tokens[index:end - 1] if not t.startswith(('//', '/*'))]
                        declaration = emit(pending + ['{'] + values + ['}'], ' '.join(comments), owner, seen,
                                           first, definition=True)
                        if declaration:
                            declaration.end = line_of(end - 1)
                            declaration.values = enum_values(index, end - 1)
                        index = end
                    else:
                        declaration = emit(pending, ' '.join(comments), owner, seen, first, definition=True)
                        index = scope(index, f'{owner}::{name}'.strip(':'),
                                      'private' if kind[1] == 'class' else 'public', visible)
                        if declaration:
                            declaration.end = line_of(index - 1)
                    pending, comments = [], []
                elif ')' in head and ('=' not in pending or pending.index('(') < pending.index('=')):
                    # Function definition, including constructors with a member initializer list.
                    # Initializer braces before the body are consumed by the fallback below.
                    if ':' in pending and pending[-1] not in ('}', ')', 'const', 'noexcept', 'override', 'final'):
                        index = balanced(index, '{', '}')
                        pending.extend(['{', '}'])
                        continue
                    signature = pending[:pending.index(':')] if ':' in pending else pending
                    emit(signature, ' '.join(comments), owner, seen, first)
                    index = balanced(index, '{', '}')
                    pending, comments = [], []
                elif head.startswith('namespace'):
                    index = scope(index, owner, 'public', visible)
                    pending, comments = [], []
                else:
                    if not pending:
                        first = index - 1
                    end = balanced(index, '{', '}')
                    pending.extend(['{'] + [t for t in tokens[index:end - 1]
                                            if not t.startswith(('//', '/*'))] + ['}'])
                    index = end
                continue
            if not pending:
                first = index - 1
            pending.append(token)
        if pending and enclosing and access == 'public':
            result.skipped += 1
        return index

    scope(0)
    return result


def public_headers(root: Path) -> list[Path]:
    headers = [path for path in (root / 'Core').rglob('*.h')
               if not {'Internals', 'Tests'} & set(path.relative_to(root).parts)]
    headers += list((root / 'Assets').rglob('*.h'))
    headers += list((root / 'Editor' / 'Api').rglob('*.h'))
    # WorkerHost is the worker entry point; the other Scene headers are host implementation.
    host = root / 'Editor/Worker/WorkerHost.h'
    if host.is_file():
        headers.append(host)
    return sorted(headers)


def parse_public(root: Path) -> list[Header]:
    """Every public header of the engine at `root`, parsed; paths are relative to `root`."""
    return [parse_header(path.relative_to(root).as_posix(), path.read_text(encoding='utf-8'))
            for path in public_headers(root)]

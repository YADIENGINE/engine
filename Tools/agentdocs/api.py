"""Heuristic C++ declaration extraction, without preprocessing or evaluating C++."""

import re
from collections import defaultdict
from dataclasses import dataclass, field
from pathlib import Path

TOKEN = re.compile(r'//[^\n]*|/\*[\s\S]*?\*/|"(?:\\.|[^"\\])*"|\'(?:\\.|[^\'\\])*\'|'
                   r'[A-Za-z_]\w*|\d+(?:\.\d+)?[\w]*|::|&&|->|\.\.\.|==|!=|<=|>=|\+=|-=|\*=|/=|\|\||[^\s]')
# Keep generated material English without editing legacy header comments. Remove an entry when translated upstream.
UNTRANSLATED = {('Core/Fx/TimeScale.h', 'TimeScale', 'void Hitstop(float seconds)'),
                ('Core/Tuning/TuningLive.h', 'TuningReport', 'std::string source')}


@dataclass
class Declaration:
    signature: str
    comment: str
    owner: str = ""


@dataclass
class Header:
    path: str
    summary: str = ""
    declarations: list[Declaration] = field(default_factory=list)
    skipped: int = 0


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

    def balanced(index: int, opening: str, closing: str) -> int:
        depth = 1
        while index < len(tokens) and depth:
            if tokens[index] == opening:
                depth += 1
            elif tokens[index] == closing:
                depth -= 1
            index += 1
        return index

    def emit(parts: list[str], comment: str, owner: str, visible: bool, definition: bool = False):
        if not visible or not parts:
            return
        signature = compact(parts).rstrip(';')
        # Large aggregate constants need a name/type, not hundreds of palette or style initializer tokens.
        if 'constexpr' in parts and '(' not in parts and '{' in parts and len(signature) > 180:
            signature = compact(parts[:parts.index('{')])
        if parts[0] in ('friend', 'static_assert'):
            return
        if navigation_noise(signature, owner, definition):
            return
        if (path, owner, signature) in UNTRANSLATED:
            comment = ''
        if len(parts) >= 2 and re.search(r'\w', signature):
            result.declarations.append(Declaration(signature, sentence(comment), owner))
        else:
            result.skipped += 1

    def scope(index: int, owner: str = '', access: str = 'public', enclosing: bool = True) -> int:
        pending: list[str] = []
        comments: list[str] = []
        parens = brackets = 0
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
            if token == '(':
                parens += 1
            elif token == ')':
                parens -= 1
            elif token == '[':
                brackets += 1
            elif token == ']':
                brackets -= 1
            if token == ';' and parens == 0 and brackets == 0:
                emit(pending, ' '.join(comments), owner, visible)
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
                        emit(pending + ['{'] + values + ['}'], ' '.join(comments), owner, visible, definition=True)
                        index = end
                    else:
                        emit(pending, ' '.join(comments), owner, visible, definition=True)
                        index = scope(index, f'{owner}::{name}'.strip(':'),
                                      'private' if kind[1] == 'class' else 'public', visible)
                    pending, comments = [], []
                elif ')' in head and ('=' not in pending or pending.index('(') < pending.index('=')):
                    # Function definition, including constructors with a member initializer list.
                    # Initializer braces before the body are consumed by the fallback below.
                    if ':' in pending and pending[-1] not in ('}', ')', 'const', 'noexcept', 'override', 'final'):
                        index = balanced(index, '{', '}')
                        pending.extend(['{', '}'])
                        continue
                    signature = pending[:pending.index(':')] if ':' in pending else pending
                    emit(signature, ' '.join(comments), owner, visible)
                    index = balanced(index, '{', '}')
                    pending, comments = [], []
                elif head.startswith('namespace'):
                    index = scope(index, owner, 'public', visible)
                    pending, comments = [], []
                else:
                    end = balanced(index, '{', '}')
                    pending.extend(['{'] + [t for t in tokens[index:end - 1]
                                            if not t.startswith(('//', '/*'))] + ['}'])
                    index = end
                continue
            pending.append(token)
        if pending and enclosing and access == 'public':
            result.skipped += 1
        return index

    scope(0)
    return result


def public_headers(root: Path) -> list[Path]:
    headers = [path for path in (root / 'Core').rglob('*.h')
               if not {'Internals', 'Tests'} & set(path.relative_to(root).parts)]
    headers += list((root / 'Editor' / 'Api').rglob('*.h'))
    # WorkerHost is the worker entry point; the other Scene headers are host implementation.
    host = root / 'Editor/Worker/WorkerHost.h'
    if host.is_file():
        headers.append(host)
    return sorted(headers)


def generate(root: Path) -> tuple[dict[str, str], list[Header]]:
    headers = [parse_header(path.relative_to(root).as_posix(), path.read_text(encoding='utf-8'))
               for path in public_headers(root)]
    areas: dict[str, list[Header]] = defaultdict(list)
    for header in headers:
        parts = header.path.split('/')
        area = parts[1] if parts[0] == 'Core' else 'Editor'
        if area == 'Graphics':
            area += '-' + parts[2]
        if area == 'Modules':
            area += '-' + parts[2]
        areas[area].append(header)
    output = {}
    for area, entries in sorted(areas.items()):
        chunks: list[list[str]] = [[]]
        for header in entries:
            lines = [f'## `{header.path}`', '']
            if header.summary:
                lines += [header.summary, '']
            for declaration in header.declarations:
                owner = f'`{declaration.owner}`: ' if declaration.owner else ''
                signature = declaration.signature.replace('`', "'")
                comment = declaration.comment.replace('\n', ' ')
                lines.append(f'- {owner}`{signature}`' + (f' — {comment}' if comment else ''))
            lines += ['']
            if chunks[-1] and len(chunks[-1]) + len(lines) > 590:
                chunks.append([])
            chunks[-1].extend(lines)
        for number, lines in enumerate(chunks, 1):
            suffix = f'-{number}' if len(chunks) > 1 else ''
            output[f'{area}{suffix}.md'] = ('Use these generated signatures to locate public declarations; open the header for details.\n\n'
                                          '<!-- Generated by Tools/agentdocs; run python . check --fix. -->\n\n'
                                          + '\n'.join(lines))
    return output, headers

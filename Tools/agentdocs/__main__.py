"""Report index size, extraction skips and uncommented public declarations."""

from collections import Counter

from ..manifest import ENGINE_ROOT
from .api import generate


def main():
    output, headers = generate(ENGINE_ROOT)
    for name, text in output.items():
        print(f'{name}: {len(text.splitlines())} lines, {len(text.encode("utf-8"))} bytes')
    print(f'Total: {sum(len(t.encode("utf-8")) for t in output.values())} bytes; '
          f'skipped declarations: {sum(h.skipped for h in headers)}')
    counts = Counter()
    missing = []
    for header in headers:
        for declaration in header.declarations:
            if not declaration.comment:
                area = header.path.split('/')[1] if header.path.startswith('Core/') else 'Editor'
                counts[area] += 1
                missing.append(f'{header.path}: {declaration.owner + "::" if declaration.owner else ""}'
                               f'{declaration.signature}')
    print('Undocumented by area: ' + ', '.join(f'{k}={v}' for k, v in sorted(counts.items())))
    for line in missing:
        print(line)


if __name__ == '__main__':
    main()

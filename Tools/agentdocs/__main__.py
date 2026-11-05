"""Report what `python Engine api` can see: declarations, extraction skips and uncommented public declarations."""

from collections import Counter

from ..manifest import ENGINE_ROOT
from .api import parse_public


def main():
    headers = parse_public(ENGINE_ROOT)
    print(f'{len(headers)} public headers, {sum(len(h.declarations) for h in headers)} declarations; '
          f'skipped declarations: {sum(h.skipped for h in headers)}')
    counts = Counter()
    missing = []
    for header in headers:
        for declaration in header.declarations:
            if not declaration.comment:
                area = header.path.split('/')[1] if header.path.startswith(('Core/', 'Assets/')) else 'Editor'
                counts[area] += 1
                missing.append(f'{header.path}:{declaration.line}: {declaration.signature}')
    print('Undocumented by area: ' + ', '.join(f'{k}={v}' for k, v in sorted(counts.items())))
    for line in missing:
        print(line)


if __name__ == '__main__':
    main()

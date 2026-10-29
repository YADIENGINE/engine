"""Shared named-sound selection; output paths remain caller-owned."""

import sys


def run(sounds, write, prefix, names, validate):
    names = (sys.argv[1:] if names is None else names) or list(sounds)
    if validate:
        unknown = [name for name in names if name not in sounds]
        if unknown:
            raise SystemExit(f"Unknown sound(s): {', '.join(unknown)}. Choose: {', '.join(sounds)}")
    for name in names:
        write(name)
        print(f"{prefix}{name}.wav")

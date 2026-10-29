"""Stable named RNG contexts, independent of draw order and Python hash seeds."""

import hashlib
import json
import operator
import secrets


def child_key(seed, scope_key, kind, name, variation):
    if not isinstance(name, str) or not name:
        raise ValueError("A scope name must be a nonempty string")
    variation = operator.index(variation)
    if variation < 0:
        raise ValueError("variation must be nonnegative")
    if scope_key is None:
        if seed is None:
            scope_key = secrets.token_bytes(32)
        else:
            try:
                number = operator.index(seed)
            except TypeError as error:
                raise TypeError("Named scopes require an integer seed or None") from error
            if number < 0:
                raise ValueError("seed must be nonnegative")
            scope_key = hashlib.sha256(f"AudioGeneration.seed.v1:{number}".encode("ascii")).digest()
    identity = json.dumps([kind, name, variation], ensure_ascii=False, separators=(",", ":")).encode("utf-8")
    digest = hashlib.sha256(scope_key + b"\x00" + identity).digest()
    return scope_key, digest, (kind, name, variation)


def describe(seed, path):
    try:
        number = operator.index(seed) if seed is not None else None
    except TypeError:
        number = None
    return {
        "seed": number,
        "scopes": [{"kind": kind, "name": name, "variation": variation} for kind, name, variation in path],
    }

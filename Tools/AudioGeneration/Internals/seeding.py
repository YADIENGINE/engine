"""Stable named RNG contexts, independent of draw order and Python hash seeds."""

import hashlib
import json
import operator
import secrets


def root(seed):
    """Validated integer root seed; None draws a fresh one that is still recorded."""
    if seed is None:
        return secrets.randbits(63)
    try:
        seed = operator.index(seed)
    except TypeError as error:
        raise ValueError("seed must be a nonnegative integer or None") from error
    if seed < 0:
        raise ValueError("seed must be a nonnegative integer or None")
    return seed


def child(seed, kind, name, variation):
    """Child seed from parent seed + scope identity (SHA-256, 63 bits)."""
    if not isinstance(name, str) or not name:
        raise ValueError("A scope name must be a nonempty string")
    variation = operator.index(variation)
    if variation < 0:
        raise ValueError("variation must be nonnegative")
    identity = json.dumps(["AudioGeneration.seed.v2", seed, kind, name, variation],
                          ensure_ascii=False, separators=(",", ":"))
    digest = hashlib.sha256(identity.encode("utf-8")).digest()
    return int.from_bytes(digest[:8], "big") >> 1, (kind, name, variation)


def describe(seed, path):
    return {"seed": seed,
            "scopes": [{"kind": kind, "name": name, "variation": variation} for kind, name, variation in path]}

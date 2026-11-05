"""`python Engine api QUERY`: find engine API by name or words instead of reading headers or searching the engine."""

import argparse
import os
import sys
from pathlib import Path

from ..agentdocs.query import answer
from ..errors import EXIT_FAILED, EXIT_OK, ToolError
from ..manifest import ENGINE_ROOT


def add_parser(commands) -> None:
    command = commands.add_parser(
        "api", help="find engine API: a name (StepsIn, CollisionWorld::Sweep) or words (fade between scenes)",
        description="Prints public declarations as path:line with their comments, at most 40 lines. A name gives the "
                    "declaration (a type also lists its members); words give the best task routes (files, recipe, "
                    "concept docs) and the declarations that share the most words.")
    command.add_argument("query", nargs="+", help="a name, Owner::Name, or words")


def display_prefix(root: Path) -> str:
    """What goes before an engine path so that it opens from the project root: `Engine/` in a game, nothing here."""
    try:
        relative = Path(os.path.relpath(ENGINE_ROOT, root)).as_posix()
    except ValueError:  # Another drive
        return ENGINE_ROOT.as_posix() + "/"
    if relative == ".":
        return ""
    return relative + "/" if not relative.startswith("..") else ENGINE_ROOT.as_posix() + "/"


def api(root: Path, args: argparse.Namespace) -> int:
    try:
        text, found = answer(ENGINE_ROOT, " ".join(args.query), display_prefix(root))
    except (ValueError, KeyError) as error:
        raise ToolError(f"AgentDocs/Tasks.json is invalid; run python Engine check ({error})") from error
    if hasattr(sys.stdout, "reconfigure"):
        sys.stdout.reconfigure(encoding="utf-8", errors="replace")  # Comments may hold non-ASCII text
    print(text)
    return EXIT_OK if found else EXIT_FAILED

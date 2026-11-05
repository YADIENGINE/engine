"""`sim`: run the project's simulation without a window (Docs/Simulation.md).

Builds the console tool made by engine_add_simulation, then runs one of its commands: list the simulations, play a
JSON scenario, verify a recorded tape against its hashes, look for nondeterminism, read the state at a step. The answer
is the tool's one JSON object on stdout; build output goes to stderr, so stdout stays readable by a script.

Settings come from the Project.json "simulation" section (Docs/ProjectSetup.md).
"""

import contextlib
import subprocess
import sys
from pathlib import Path

from ..cmake import BUILD_LOG, PYTHON_DEFINITION, clear_stale_build, read_cache, run
from ..errors import EXIT_INFRA, ToolError
from ..msvc import msvc_env, tool
from ..prebuilt import cache_definition, prebuilt_directory, write_state
from ..project import Project
from .build import CONFIGURE_STAMP, config_name, needs_configure

SECTION = "simulation"


def add_parser(commands) -> None:
    command = commands.add_parser("sim", help="run the project's simulation without a window: scenarios and replays")
    actions = command.add_subparsers(dest="action", required=True, metavar="ACTION")

    def action(name: str, help_text: str):
        parser = actions.add_parser(name, help=help_text)
        parser.add_argument("--release", action="store_true", help="the Release build of the tool")
        return parser

    action("list", "the project's simulations with their actions")
    parser = action("run", "play a JSON scenario; exit 1 when an expectation fails")
    parser.add_argument("scenario", metavar="SCENARIO")
    parser.add_argument("--out", metavar="TAPE", help="write the played run as a tape")
    parser.add_argument("--events", action="store_true", help="list every event the simulation emitted")
    action("verify", "replay a tape and compare its hashes; exit 1 at the first difference").add_argument(
        "tape", metavar="TAPE")
    action("diverge", "replay a tape twice and name the first differing step and fields").add_argument(
        "tape", metavar="TAPE")
    parser = action("state", "the state after a step of a tape; default: its end")
    parser.add_argument("tape", metavar="TAPE")
    parser.add_argument("--step", type=int, metavar="N")


def has_sources(project: Project, section: dict) -> bool:
    folder = project.path(section.get("sources", "Game/Simulation"))
    return folder.is_dir() and any(folder.rglob("*.cpp"))


def build_tool(project: Project, section: dict, release: bool) -> Path:
    """Compiles the console tool and returns its path. Output goes to stderr: stdout carries only the answer."""
    config = config_name(release)
    build_dir = project.path(section.get("buildDirectory", "build/simulation"))
    prebuilt, state = prebuilt_directory(project)
    clear_stale_build(build_dir, project.root)
    write_state(project, state)
    env = msvc_env(project.msvc_cache)
    definition = cache_definition(prebuilt)
    cached = (read_cache(build_dir) or {}).get("ENGINE_PREBUILT_DIR", "")
    with contextlib.redirect_stdout(sys.stderr):
        if needs_configure(project.root, build_dir) or cached != definition.split("=", 1)[1]:
            run([tool("cmake"), "--preset", section["configurePreset"], PYTHON_DEFINITION, definition], project.root,
                env, log=build_dir / BUILD_LOG)
            (build_dir / CONFIGURE_STAMP).touch()
        run([tool("cmake"), "--build", "--preset", section["buildPresets"][config]], project.root, env,
            log=build_dir / BUILD_LOG)
    return build_dir / config / (section.get("target", f"{project.name}SimulationTool") + ".exe")


def tool_arguments(args) -> list[str]:
    """The console tool's command line; relative paths mean the caller's folder, and the tool runs there too."""
    match args.action:
        case "list":
            return ["list"]
        case "run":
            return (["run", args.scenario] + (["--out", args.out] if args.out else [])
                    + (["--events"] if args.events else []))
        case "state":
            return ["state", args.tape] + (["--step", str(args.step)] if args.step is not None else [])
        case _:
            return [args.action, args.tape]


def sim(project: Project, args) -> int:
    section = project.setting(SECTION, "configurePreset") and project.section(SECTION)
    if not section:
        raise ToolError(f'Project.json has no "{SECTION}" section; see Engine/Docs/Simulation.md', EXIT_INFRA)
    if not has_sources(project, section):
        raise ToolError(f"No simulation sources under {section.get('sources', 'Game/Simulation')}; "
                        "see Engine/Docs/Simulation.md", EXIT_INFRA)
    executable = build_tool(project, section, args.release)
    return subprocess.run([str(executable), *tool_arguments(args)]).returncode

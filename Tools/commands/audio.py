"""`audio`: facts about the project's sound files for tools and agents, without the editor (Docs/AudioTools.md).

Decoding needs C++, so the project's editor worker answers: `<worker> --audio-report REQUEST.json` reads the files,
writes an answer file and exits, without a window or an audio device. The worker also reports the project's
`AudioCatalog`, which says which files the game plays. Build the worker once with `python Engine editor --build-only`;
a catalog changed since then shows after the next build (or the editor's Build & Refresh).

`info` gives length, format, levels, silent edges and clipping; `waveform` draws a PNG to look at; `usage` lists the
sound files with the catalog entries that play them (`--unused`, `--missing`); `catalog` prints the catalog itself.
"""

import argparse
import json
import os
import subprocess
from pathlib import Path

from ..errors import EXIT_FAILED, EXIT_OK, ToolError
from ..project import Project
from .editor import PROJECT_RECIPE, editor_build_dir

AUDIO_EXTENSIONS = {".ogg", ".wav", ".mp3", ".flac"}  # As the editor's asset library lists sounds
REPORT_FOLDER = "audio-report"
REPORT_TIMEOUT = 300  # Seconds; decoding long music files is the slow part
CONFIGS = ("Debug", "Release")


def add_parser(commands) -> None:
    command = commands.add_parser("audio", help="sound files: facts, waveform image, catalog usage (no editor needed)")
    actions = command.add_subparsers(dest="audio_action", required=True, metavar="<action>")
    action = actions.add_parser("info", help="length, format, peak/RMS dBFS, silent edges, clipping, catalog uses")
    action.add_argument("files", nargs="+", metavar="FILE",
                        help="absolute, or relative to the current folder, project root or asset folder")
    action.add_argument("--silence-db", type=float, default=-60.0,
                        help="quieter than this counts as silence at the edges (default -60)")
    action = actions.add_parser("waveform", help="a PNG of the waveform to look at; faint lines every tickSeconds")
    action.add_argument("file", metavar="FILE")
    action.add_argument("--out", help="PNG path (default: audio-report/<name>-waveform.png in the editor build)")
    action.add_argument("--width", type=int, default=1200)
    action.add_argument("--height", type=int, default=240)
    action = actions.add_parser("usage", help="sound files and the catalog entries that play them")
    action.add_argument("folder", nargs="?", help="only this folder (default: the whole asset folder)")
    which = action.add_mutually_exclusive_group()
    which.add_argument("--unused", action="store_true", help="only files no catalog entry plays")
    which.add_argument("--missing", action="store_true", help="only catalog entries whose file does not exist")
    actions.add_parser("catalog", help="the project's audio catalog: ids, names, files, settings")


def rounded(value):
    """Floats to three decimals (seconds to the millisecond, dB to a thousandth), recursively."""
    if isinstance(value, float):
        return round(value, 3)
    if isinstance(value, dict):
        return {key: rounded(item) for key, item in value.items()}
    if isinstance(value, list):
        return [rounded(item) for item in value]
    return value


def use_label(kind: str, use: dict) -> str:
    """"Effect Click" or "Music #3", as the editor shows a catalog entry."""
    title = "Music" if kind == "music" else "Effect"
    return f"{title} {use['name']}" if use.get("name") else f"{title} #{use['id']}"


def catalog_uses(catalog: dict) -> list[dict]:
    """Every catalog entry as {kind, id, name, file, label}; `file` is relative to the asset folder."""
    uses = []
    for key, kind in (("effects", "effect"), ("music", "music")):
        for entry in catalog.get(key, []):
            uses.append({"kind": kind, "id": entry["id"], "name": entry.get("name", ""), "file": entry["file"],
                         "label": use_label(kind, entry)})
    return uses


def comparable(path: str) -> str:
    """Catalog paths and disk paths compare without case (Windows) and with `/`."""
    return os.path.normpath(path).replace("\\", "/").casefold()


def uses_of(uses: list[dict], asset: str) -> list[dict]:
    key = comparable(asset)
    return [use for use in uses if comparable(use["file"]) == key]


def sound_files(folder: Path) -> list[Path]:
    return sorted((path for path in folder.rglob("*") if path.is_file() and path.suffix.lower() in AUDIO_EXTENSIONS),
                  key=lambda path: path.as_posix().casefold())


def usage_answer(catalog: dict, assets: Path, folder: Path, unused: bool = False, missing: bool = False) -> dict:
    """Sound files under `folder` with the catalog entries that play them, plus entries whose file is gone."""
    uses = catalog_uses(catalog)
    sounds = []
    for path in sound_files(folder):
        asset = path.relative_to(assets).as_posix()
        sounds.append({"asset": asset, "uses": [use["label"] for use in uses_of(uses, asset)]})
    gone = [use for use in uses if not (assets / use["file"]).is_file()]
    if missing:
        return {"missing": gone}
    if unused:
        return {"unused": [sound["asset"] for sound in sounds if not sound["uses"]]}
    return {"sounds": sounds, "unused": sum(1 for sound in sounds if not sound["uses"]), "missing": gone}


def editor_recipe(project: Project) -> dict:
    """The editor build's project recipe with a built worker (the newer configuration when both exist)."""
    build_dir = editor_build_dir(project)
    found = []
    for config in CONFIGS:
        recipe = build_dir / "Editor" / config / PROJECT_RECIPE
        if not recipe.is_file():
            continue
        data = json.loads(recipe.read_text(encoding="utf-8"))
        worker = Path(data["worker"]["executable"])
        if worker.is_file():
            found.append((worker.stat().st_mtime, data))
    if not found:
        raise ToolError("No editor worker is built yet; run: python Engine editor --build-only")
    return max(found, key=lambda item: item[0])[1]


def resolve_sound(text: str, project: Project, assets: Path) -> Path:
    """A sound file named absolutely or relative to the current folder, the project root or the asset folder."""
    path = Path(text)
    candidates = [path] if path.is_absolute() else [Path.cwd() / path, project.root / path, assets / path]
    for candidate in candidates:
        if candidate.is_file():
            return candidate.resolve()
    raise ToolError(f"No such sound file: {text} (tried {', '.join(str(c) for c in candidates)})", EXIT_FAILED)


def run_report(project: Project, recipe: dict, files: list[Path], silence_db: float | None = None,
               waveform: dict | None = None) -> dict:
    """Asks the worker; returns its answer ({version, catalog, files, waveform})."""
    folder = editor_build_dir(project) / REPORT_FOLDER
    folder.mkdir(parents=True, exist_ok=True)
    request_file = folder / f"request-{os.getpid()}.json"
    answer_file = folder / f"answer-{os.getpid()}.json"
    request = {"out": str(answer_file), "files": [str(file) for file in files]}
    if silence_db is not None:
        request["silenceDb"] = silence_db
    if waveform is not None:
        request["waveform"] = waveform
    request_file.write_text(json.dumps(request), encoding="utf-8")
    worker = recipe["worker"]
    try:
        result = subprocess.run([worker["executable"], *worker.get("arguments", []), "--audio-report",
                                 str(request_file)], cwd=worker.get("directory") or None, capture_output=True,
                                text=True, encoding="utf-8", errors="replace", timeout=REPORT_TIMEOUT)
        if result.returncode != 0:
            detail = (result.stderr.strip().splitlines() or [f"exit {result.returncode}"])[-1]
            raise ToolError(f"The editor worker could not answer: {detail}", EXIT_FAILED)
        return json.loads(answer_file.read_text(encoding="utf-8"))
    except subprocess.TimeoutExpired as error:
        raise ToolError(f"The editor worker did not answer within {REPORT_TIMEOUT} s", EXIT_FAILED) from error
    finally:
        for leftover in (request_file, answer_file):
            leftover.unlink(missing_ok=True)


def with_assets(entry: dict, uses: list[dict], assets: Path) -> dict:
    """A file entry with its path in the asset folder and the catalog entries that play it."""
    path = Path(entry["file"])
    try:
        asset = path.resolve().relative_to(assets).as_posix()
    except ValueError:
        return entry | {"asset": None, "uses": []}  # Outside the asset folder: the game cannot load it
    return entry | {"asset": asset, "uses": [use["label"] for use in uses_of(uses, asset)]}


def audio(project: Project, args: argparse.Namespace) -> int:
    recipe = editor_recipe(project)
    assets = Path(recipe["assetsDirectory"]).resolve()
    match args.audio_action:
        case "info":
            files = [resolve_sound(text, project, assets) for text in args.files]
            report = run_report(project, recipe, files, silence_db=args.silence_db)
            uses = catalog_uses(report["catalog"])
            answer = {"files": [with_assets(entry, uses, assets) for entry in report["files"]]}
        case "waveform":
            file = resolve_sound(args.file, project, assets)
            out = Path(args.out).resolve() if args.out else (
                editor_build_dir(project) / REPORT_FOLDER / f"{file.stem}-waveform.png")
            report = run_report(project, recipe, [], waveform={"file": str(file), "path": str(out),
                                                               "width": args.width, "height": args.height})
            answer = report["waveform"]
        case "usage":
            folder = resolve_folder(args.folder, project, assets) if args.folder else assets
            report = run_report(project, recipe, [])
            answer = usage_answer(report["catalog"], assets, folder, args.unused, args.missing)
        case "catalog":
            answer = {"uses": catalog_uses(run_report(project, recipe, [])["catalog"])}
        case _:
            raise AssertionError(f"unhandled audio action {args.audio_action}")
    print(json.dumps(rounded(answer), ensure_ascii=False, indent=2), flush=True)
    failed = any("error" in entry for entry in answer.get("files", []))
    return EXIT_FAILED if failed else EXIT_OK


def resolve_folder(text: str, project: Project, assets: Path) -> Path:
    path = Path(text)
    candidates = [path] if path.is_absolute() else [Path.cwd() / path, project.root / path, assets / path]
    for candidate in candidates:
        if candidate.is_dir():
            folder = candidate.resolve()
            if not folder.is_relative_to(assets):
                raise ToolError(f"{text} is outside the asset folder {assets}", EXIT_FAILED)
            return folder
    raise ToolError(f"No such folder: {text}", EXIT_FAILED)

"""`release`: Release derlemesinden paket klasörü + yanına ZIP: exe, seçili Assets, ThirdPartyNotices.txt, ek dosyalar.

Ayarlar Project.json "release" bölümünde (bkz. Docs/Release.md). Bildirimin engine kısmı ThirdParty/notices.py'den,
oyun kısmı projenin `release.notices` dosyasından gelir. Bir şey eksikse ya da engine bildirimi güncel değilse
paketleme mevcut paketi silmeden durur. Sistem büyürse (imza, mağaza depot'u) bu modül alt pakete dönüşür.
"""

import os
import hashlib
import json
import shutil
import subprocess
from pathlib import Path

from ThirdParty.notices import check_components, compose_notices, engine_notice, load_components, scoped_components

from ..errors import EXIT_FAILED, ToolError
from ..manifest import ENGINE_ROOT, load_manifest
from ..project import Project
from ..crash_reports import REPORTER, pe_identity, pdb_identity
from .audio import comparable, sound_files

ENGINE_SOUNDS = Path("Engine")  # The engine's own assets in a built Assets folder
ASSET_REPORT_VARIABLE = "ENGINE_ASSET_REPORT"  # Keep in step with Core/Runtime/Game.cpp
ASSET_REPORT_PREFIX = "asset: "
ASSET_REPORT_TIMEOUT = 60  # Seconds; the game prints its list before opening a window
NOTICES_FILE = "ThirdPartyNotices.txt"  # Paketin kökünde üretilen bildirim
ENGINE_LICENSE = ENGINE_ROOT / "LICENSE"  # Engine'in kendi lisansı; bildirimin Part 1'inin başına girer


def symbol_inputs(exe: Path) -> tuple[dict, Path, Path]:
    try:
        identity = pe_identity(exe)
    except (OSError, ToolError) as error:
        raise ToolError(f"Release stopped: {error}", EXIT_FAILED) from error
    pdb = exe.parent / identity["name"]
    helper = exe.parent / REPORTER
    if not pdb.is_file() or not helper.is_file():
        raise ToolError(f"Release stopped: matching PDB and {REPORTER} must be built beside {exe}", EXIT_FAILED)
    try:
        actual = pdb_identity(pdb)
    except (OSError, ToolError) as error:
        raise ToolError(f"Release stopped: {error}", EXIT_FAILED) from error
    if any(actual[key] != identity[key] for key in ("guid", "age")):
        raise ToolError(f"Release stopped: {pdb} does not match {exe}", EXIT_FAILED)
    return identity, pdb, helper


def archive_symbols(root: Path, exe: Path, identity: dict, pdb: Path, helper: Path) -> Path:
    fingerprint = hashlib.sha256(exe.read_bytes() + pdb.read_bytes()).hexdigest()
    target = root / "Symbols" / f"{identity['guid']}-{identity['age']}-{fingerprint[:16]}"
    target.mkdir(parents=True, exist_ok=True)
    for source in (exe, pdb, helper):
        destination = target / source.name
        if destination.exists() and destination.read_bytes() != source.read_bytes():
            raise ToolError(f"Symbol archive collision: {destination}", EXIT_FAILED)
        if not destination.exists():
            shutil.copy2(source, destination)
    (target / "Symbols.json").write_text(json.dumps({"schema_version": 1, "executable": exe.name,
        "pdb": identity, "sha256": fingerprint}, indent=2) + "\n", encoding="utf-8")
    return target


def copy_entry(source: Path, destination: Path) -> None:
    destination.parent.mkdir(parents=True, exist_ok=True)
    if source.is_dir():
        shutil.copytree(source, destination)
    else:
        shutil.copy2(source, destination)


def asset_report(exe: Path) -> list[str]:
    """The files the built game uses: its asset manifest, relative to Assets. Only the running code knows them."""
    try:
        result = subprocess.run([str(exe)], cwd=exe.parent, env=os.environ | {ASSET_REPORT_VARIABLE: "1"},
                                capture_output=True, encoding="utf-8", errors="replace",
                                timeout=ASSET_REPORT_TIMEOUT, check=False)
    except (OSError, subprocess.TimeoutExpired) as error:
        raise ToolError(f"Release stopped: could not ask {exe.name} which assets it uses: {error}",
                        EXIT_FAILED) from error
    if result.returncode != 0:
        detail = (result.stderr.strip().splitlines() or [f"exit {result.returncode}"])[-1]
        raise ToolError(f"Release stopped: could not ask {exe.name} which assets it uses: {detail}", EXIT_FAILED)
    return [line.removeprefix(ASSET_REPORT_PREFIX) for line in result.stdout.splitlines()
            if line.startswith(ASSET_REPORT_PREFIX)]


def kept_by(entry: str, keep: list[str]) -> bool:
    """Whether release.keepAssets names this file or a folder above it."""
    key = comparable(entry)
    return any(key == comparable(item) or key.startswith(comparable(item) + "/") for item in keep)


def select_sounds(exe: Path, built_assets: Path, keep: list[str]) -> tuple[list[Path], list[str]]:
    """The built sound files the package leaves out, and the problems found on the way."""
    errors = [f"Missing package file: {built_assets / entry} (release.keepAssets)" for entry in keep
              if not (built_assets / entry).exists()]
    sounds = sound_files(built_assets)
    if not sounds:
        return [], errors
    report = asset_report(exe)
    errors += [f"Missing package file: {built_assets / entry} (the game uses it)" for entry in report
               if not (built_assets / entry).is_file()]
    used = {comparable(entry) for entry in report}
    left_out = []
    for sound in sounds:
        entry = sound.relative_to(built_assets).as_posix()
        if comparable(entry) not in used and not kept_by(entry, keep):
            left_out.append(sound)
    return left_out, errors


def print_left_out(left_out: list[Path], built_assets: Path) -> None:
    """Each of the game's sounds by name, since one loaded by code must be named in release.keepAssets."""
    game = [sound for sound in left_out if not sound.relative_to(built_assets).is_relative_to(ENGINE_SOUNDS)]
    if game:
        megabytes = sum(sound.stat().st_size for sound in game) / (1024 * 1024)
        print(f"> {len(game)} unused sound(s) left out ({megabytes:.1f} MB); "
              "one that code loads by itself goes in release.keepAssets:", flush=True)
        for sound in game:
            print(f"    {sound.relative_to(built_assets).as_posix()}", flush=True)
    if len(left_out) > len(game):
        print(f"> {len(left_out) - len(game)} unused engine sound(s) left out", flush=True)


def package(project: Project, exe: Path) -> Path:
    """Paket klasörünü kurar, yanına ZIP alır ve paketteki exe'yi döndürür. Assets'ten ne gireceğini projenin
    `package_assets(root)` hook'u seçer; hook yoksa Assets'in tamamı girer, oyunun kullanmadığı sesler hariç."""
    settings = project.section("release")
    identity, pdb, helper = symbol_inputs(exe)
    name = settings.get("name", project.name)
    package_root = project.path(settings.get("directory", "Release"))
    target = package_root / name
    symbol_root = (package_root / "Symbols").resolve()
    resolved_target = target.resolve()
    if resolved_target.is_relative_to(symbol_root) or symbol_root.is_relative_to(resolved_target):
        raise ToolError("Release stopped: package name overlaps the retained Symbols directory; choose release.name",
                        EXIT_FAILED)
    built_assets = exe.parent / "Assets"
    select = project.hook("package_assets") if built_assets.is_dir() else None
    assets = select(project.root) if select is not None else []
    starter_font = Path("Fonts/Starter.ttf")
    starter = (built_assets / starter_font).is_file() and (
        select is None or any(Path(entry) == starter_font or Path(entry) in starter_font.parents for entry in assets))
    manifest = scoped_components(load_components(), starter=starter)
    errors = check_components(manifest)
    if not ENGINE_LICENSE.is_file():
        errors.append(f"Missing engine license: {ENGINE_LICENSE}")
    notices = settings.get("notices")
    if notices is not None and not project.path(notices).is_file():
        errors.append(f"Missing game notices: {project.path(notices)}")
    extras = [project.path(entry) for entry in settings.get("files", [])]
    errors += [f"Missing package file: {extra}" for extra in extras if not extra.is_file()]
    errors += [f"{extra}: {NOTICES_FILE} is generated; list the game's part as release.notices"
               for extra in extras if extra.name == NOTICES_FILE]
    errors += [f"{extra}: {REPORTER} is supplied by the build and cannot be replaced by release.files"
               for extra in extras if extra.name.casefold() == REPORTER.casefold()]
    errors += [f"Missing package file: {built_assets / entry}" for entry in assets
               if not (built_assets / entry).exists()]
    keep = settings.get("keepAssets", [])
    left_out = []
    if select is not None and keep:
        errors.append("release.keepAssets has no effect with a package_assets hook; the hook selects every asset")
    elif select is None and built_assets.is_dir():
        left_out, sound_errors = select_sounds(exe, built_assets, keep)
        errors += sound_errors
    if errors:
        raise ToolError("Release stopped:\n  " + "\n  ".join(errors), EXIT_FAILED)

    symbols = archive_symbols(package_root, exe, identity, pdb, helper)

    game_text = project.path(notices).read_text(encoding="utf-8") if notices is not None else None
    print(f"> package {target}", flush=True)
    if target.exists():
        shutil.rmtree(target)
    target.mkdir(parents=True)

    shutil.copy2(exe, target / exe.name)
    shutil.copy2(helper, target / helper.name)
    engine = load_manifest()
    engine_text = engine_notice(engine.name, engine.version, ENGINE_LICENSE.read_text(encoding="utf-8"))
    (target / NOTICES_FILE).write_text(compose_notices(manifest, name, game_text, engine_text=engine_text),
                                       encoding="utf-8", newline="\n")
    for extra in extras:
        shutil.copy2(extra, target / extra.name)
    if select is not None:
        for entry in assets:
            copy_entry(built_assets / entry, target / "Assets" / entry)
    elif built_assets.is_dir():
        skipped = set(left_out)
        for file in built_assets.rglob("*"):
            if file.is_file() and file not in skipped:
                copy_entry(file, target / "Assets" / file.relative_to(built_assets))
        print_left_out(left_out, built_assets)

    archive = shutil.make_archive(str(target), "zip", target.parent, target.name)
    print(f"> {archive}", flush=True)
    print(f"Symbols: {symbols}", flush=True)
    return target / exe.name

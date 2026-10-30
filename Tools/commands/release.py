"""`release`: Release derlemesinden paket klasörü + yanına ZIP: exe, seçili Assets, ThirdPartyNotices.txt, ek dosyalar.

Ayarlar Project.json "release" bölümünde (bkz. Docs/Release.md). Bildirimin engine kısmı ThirdParty/notices.py'den,
oyun kısmı projenin `release.notices` dosyasından gelir. Bir şey eksikse ya da engine bildirimi güncel değilse
paketleme mevcut paketi silmeden durur. Sistem büyürse (imza, mağaza depot'u) bu modül alt pakete dönüşür.
"""

import shutil
from pathlib import Path

from ThirdParty.notices import check_components, compose_notices, load_components

from ..errors import EXIT_FAILED, ToolError
from ..project import Project

NOTICES_FILE = "ThirdPartyNotices.txt"  # Paketin kökünde üretilen bildirim


def copy_entry(source: Path, destination: Path) -> None:
    destination.parent.mkdir(parents=True, exist_ok=True)
    if source.is_dir():
        shutil.copytree(source, destination)
    else:
        shutil.copy2(source, destination)


def package(project: Project, exe: Path) -> Path:
    """Paket klasörünü kurar, yanına ZIP alır ve paketteki exe'yi döndürür. Assets'ten ne gireceğini projenin
    `package_assets(root)` hook'u seçer; hook yoksa Assets'in tamamı girer."""
    settings = project.section("release")
    name = settings.get("name", project.name)
    manifest = load_components()
    errors = check_components(manifest)
    notices = settings.get("notices")
    if notices is not None and not project.path(notices).is_file():
        errors.append(f"Missing game notices: {project.path(notices)}")
    extras = [project.path(entry) for entry in settings.get("files", [])]
    errors += [f"Missing package file: {extra}" for extra in extras if not extra.is_file()]
    errors += [f"{extra}: {NOTICES_FILE} is generated; list the game's part as release.notices"
               for extra in extras if extra.name == NOTICES_FILE]
    built_assets = exe.parent / "Assets"
    select = project.hook("package_assets") if built_assets.is_dir() else None
    assets = select(project.root) if select is not None else []
    errors += [f"Missing package file: {built_assets / entry}" for entry in assets
               if not (built_assets / entry).exists()]
    if errors:
        raise ToolError("Release stopped:\n  " + "\n  ".join(errors), EXIT_FAILED)

    game_text = project.path(notices).read_text(encoding="utf-8") if notices is not None else None
    target = project.path(settings.get("directory", "Release")) / name
    print(f"> package {target}", flush=True)
    if target.exists():
        shutil.rmtree(target)
    target.mkdir(parents=True)

    shutil.copy2(exe, target / exe.name)
    (target / NOTICES_FILE).write_text(compose_notices(manifest, name, game_text), encoding="utf-8", newline="\n")
    for extra in extras:
        shutil.copy2(extra, target / extra.name)
    if select is not None:
        for entry in assets:
            copy_entry(built_assets / entry, target / "Assets" / entry)
    elif built_assets.is_dir():
        shutil.copytree(built_assets, target / "Assets")

    archive = shutil.make_archive(str(target), "zip", target.parent, target.name)
    print(f"> {archive}", flush=True)
    return target / exe.name

"""Release paketi: exe + seçili Assets + üretilmiş ThirdPartyNotices.txt + ek dosyalar, yanına ZIP.

build_cli `p.py --release` ile çağırır; ayarlar Project.json `release` bölümünde (bkz. Docs/Release.md).
Bildirimin engine kısmı ThirdParty/notices.py'den (bileşenler ThirdParty/Notices.json), oyun kısmı projenin
`release.notices` dosyasından gelir. Engine bildirimi güncel değilse paketleme durur.
"""

import shutil
import sys
from pathlib import Path

ENGINE = Path(__file__).resolve().parents[1]
sys.path.insert(0, str(ENGINE / "ThirdParty"))

from notices import check_components, compose_notices, load_components  # noqa: E402

NOTICES_FILE = "ThirdPartyNotices.txt"  # Paketin kökünde üretilen bildirim


def copy_entry(source: Path, destination: Path) -> None:
    destination.parent.mkdir(parents=True, exist_ok=True)
    if source.is_dir():
        shutil.copytree(source, destination)
    else:
        shutil.copy2(source, destination)


def package(project, exe: Path) -> Path:
    """Paket klasörünü kurar, yanına ZIP alır ve paketteki exe'yi döndürür. Bir şey eksik ya da bildirim eskiyse
    mevcut paket silinmeden durur. Assets'ten ne gireceğini projenin `package_assets(root)` hook'u seçer."""
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
        sys.exit("Release stopped:\n  " + "\n  ".join(errors))

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

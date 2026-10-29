"""Release paketi: exe + seçili Assets + üretilmiş ThirdPartyNotices.txt + ek dosyalar, yanına ZIP.

build_cli `p.py --release` ile çağırır; ayarlar Project.json `release` bölümünde (bkz. Docs/Release.md).
Bildirimin Engine kısmı yanındaki Notices.json'daki bileşenlerden, oyun kısmı projenin `release.notices`
dosyasından gelir. Bileşen sürümü Engine'deki pin'le ya da metni indirilen kaynağın lisansıyla uyuşmazsa
paketleme durur.
"""

import json
import re
import shutil
import sys
import textwrap
from pathlib import Path

ENGINE = Path(__file__).resolve().parents[1]
SYSTEM = Path("ReleaseSystem")  # Engine'e göre bu klasör; Notices.json ve ThirdParty/ burada
NOTICES = SYSTEM / "Notices.json"
NOTICES_FILE = "ThirdPartyNotices.txt"  # Paketin kökünde üretilen bildirim
TITLE = "THIRD-PARTY NOTICES"
PREAMBLE = (
    "This distribution includes third-party software and content. The notices below apply only to the\n"
    "identified components; they do not grant rights to the rest of the game. Where a component offers\n"
    "several licenses, the selected option is named in its heading."
)
RULE = "=" * 80


def load_components(engine: Path = ENGINE) -> dict:
    return json.loads((engine / NOTICES).read_text(encoding="utf-8"))


def normalized(text: str) -> str:
    return re.sub(r"\s+", " ", text).strip()


def check_components(manifest: dict, engine: Path = ENGINE, deps: Path | None = None) -> list[str]:
    """Bildirimin güncel olmadığını gösteren hatalar. `deps` FetchContent'in `_deps` klasörüdür; varsa kayıtlı
    metnin indirilen kaynağın lisansını aynen içerdiği de denetlenir."""
    errors = []
    for component in manifest["components"]:
        name, version = component["name"], component["version"]
        text_path = engine / SYSTEM / component["text"]
        if not text_path.is_file():
            errors.append(f"{name}: missing notice text {text_path}")
            continue
        text = text_path.read_text(encoding="utf-8")
        if version not in text.splitlines()[0]:
            errors.append(f"{name}: first line of {text_path.name} does not name version {version}")
        pin = component["pin"]
        pinned = pin["contains"].format(version=version)
        pin_path = engine / pin["file"]
        if not pin_path.is_file() or pinned not in pin_path.read_text(encoding="utf-8"):
            errors.append(f"{name} {version}: '{pinned}' not found in {pin_path}; the dependency changed, "
                          f"update {NOTICES.as_posix()} and the notice text")
        upstream = component.get("upstreamLicense")
        if upstream and deps is not None and (deps / upstream).is_file():
            if normalized((deps / upstream).read_text(encoding="utf-8")) not in normalized(text):
                errors.append(f"{name}: {text_path.name} does not contain the downloaded license {deps / upstream}")
    return errors


def section(title: str, body: str) -> str:
    return f"{RULE}\n{title}\n{RULE}\n\n{body.strip()}\n"


def compose_notices(manifest: dict, game_name: str, game_text: str | None, engine: Path = ENGINE) -> str:
    texts = [(engine / SYSTEM / c["text"]).read_text(encoding="utf-8").strip() for c in manifest["components"]]
    parts = [f"{TITLE}\n{'=' * len(TITLE)}\n\n{PREAMBLE}\n",
             section("PART 1: ENGINE", textwrap.fill(manifest["intro"], 100) +"\n\n" + "\n\n".join(texts))]
    if game_text is not None:
        parts.append(section(f"PART 2: {game_name.upper()}", game_text))
    return "\n".join(parts)


def copy_entry(source: Path, destination: Path) -> None:
    destination.parent.mkdir(parents=True, exist_ok=True)
    if source.is_dir():
        shutil.copytree(source, destination)
    else:
        shutil.copy2(source, destination)


def package(project, exe: Path, build_dir: Path) -> Path:
    """Paket klasörünü kurar, yanına ZIP alır ve paketteki exe'yi döndürür. Bir şey eksik ya da bildirim eskiyse
    mevcut paket silinmeden durur. Assets'ten ne gireceğini projenin `package_assets(root)` hook'u seçer."""
    settings = project.section("release")
    name = settings.get("name", project.name)
    manifest = load_components()
    errors = check_components(manifest, deps=build_dir / "_deps")
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

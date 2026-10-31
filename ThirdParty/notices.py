"""Oyuna giren bağımlılıkların lisans bildirimi: denetim ve ThirdPartyNotices.txt'nin engine kısmı.

Bileşen listesi yanındaki Notices.json'dadır; yollar bu klasöre (ThirdParty/) göredir. Bir bileşenin sürümü
kaynağındaki pin'le uyuşmazsa ya da bildirim metni kaynağın lisansını aynen içermiyorsa denetim hata döndürür.
ReleaseSystem/release.py paketlerken bunu kullanır. Editör ve test bağımlılıkları oyuna girmez, listede yoktur.
"""

import json
import re
import textwrap
from pathlib import Path

THIRD_PARTY = Path(__file__).resolve().parent
NOTICES = "Notices.json"
TITLE = "THIRD-PARTY NOTICES"
PREAMBLE = (
    "This distribution includes third-party software and content. The notices below apply only to the\n"
    "identified components; they do not grant rights to the rest of the game. Where a component offers\n"
    "several licenses, the selected option is named in its heading."
)
RULE = "=" * 80


def load_components(third_party: Path = THIRD_PARTY) -> dict:
    return json.loads((third_party / NOTICES).read_text(encoding="utf-8"))


def normalized(text: str) -> str:
    return re.sub(r"\s+", " ", text).strip()


def scoped_components(manifest: dict, *, starter: bool = False) -> dict:
    """Select notices for a distribution; unscoped components ship in every application."""
    return {**manifest, "components": [c for c in manifest["components"]
                                      if c.get("scope") != "starter" or starter]}


def check_components(manifest: dict, third_party: Path = THIRD_PARTY) -> list[str]:
    """Bildirimin güncel olmadığını gösteren hatalar."""
    errors = []
    for component in manifest["components"]:
        name, version = component["name"], component["version"]
        if component.get("scope") not in (None, "starter"):
            errors.append(f"{name}: unknown notice scope {component['scope']!r}")
        text_path = third_party / component["text"]
        if not text_path.is_file():
            errors.append(f"{name}: missing notice text {text_path}")
            continue
        text = text_path.read_text(encoding="utf-8")
        if version not in text.splitlines()[0]:
            errors.append(f"{name}: first line of {text_path.name} does not name version {version}")
        pin = component["pin"]
        pinned = pin["contains"].format(version=version)
        pin_path = third_party / pin["file"]
        if not pin_path.is_file() or pinned not in pin_path.read_text(encoding="utf-8"):
            errors.append(f"{name} {version}: '{pinned}' not found in {pin_path}; the dependency changed, "
                          f"update ThirdParty/{NOTICES} and the notice text")
        license_file = component.get("license")
        if license_file is not None:
            license_path = third_party / license_file
            if not license_path.is_file():
                errors.append(f"{name}: missing license {license_path}")
            elif normalized(license_path.read_text(encoding="utf-8")) not in normalized(text):
                errors.append(f"{name}: {text_path.name} does not contain the license {license_path}")
    return errors


def section(title: str, body: str) -> str:
    return f"{RULE}\n{title}\n{RULE}\n\n{body.strip()}\n"


def engine_notice(name: str, version: str, license_text: str) -> str:
    """Engine'in kendi lisansı, bileşen metinleriyle aynı biçimde (ad + sürüm başlığı, sonra lisans)."""
    title = f"{name} {version} (MIT)"
    return f"{title}\n{'-' * len(title)}\n\n{license_text.strip()}"


def compose_notices(manifest: dict, game_name: str, game_text: str | None, third_party: Path = THIRD_PARTY,
                    engine_text: str | None = None) -> str:
    texts = [(third_party / c["text"]).read_text(encoding="utf-8").strip() for c in manifest["components"]]
    if engine_text is not None:
        texts.insert(0, engine_text.strip())
    parts = [f"{TITLE}\n{'=' * len(TITLE)}\n\n{PREAMBLE}\n",
             section("PART 1: ENGINE", textwrap.fill(manifest["intro"], 100) + "\n\n" + "\n\n".join(texts))]
    if game_text is not None:
        parts.append(section(f"PART 2: {game_name.upper()}", game_text))
    return "\n".join(parts)

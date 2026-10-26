"""YDArcadeEngine ile yeni bir oyun projesi kurar.

Kullanım
--------
Proje klasörüne sadece Engine'i koyun (klasörün adı tam olarak "Engine" olmalı), sonra Engine'in içinde:

    python EngineSetup.py              Proje adını sorar, proje dosyalarını bir üst klasöre yazar
    python EngineSetup.py --dry-run    Hiçbir şey yazmadan neyin oluşturulacağını listeler

Ne yapar (sırayla)
------------------
1. Klasör adını denetler. Proje CMake'i `add_subdirectory(Engine)` der, p.py / editor.py `Engine/Tools`'u yükler,
   Android Gradle `Engine/...` yollarını okur; bu yüzden ad "Engine" olmak zorunda. GitHub'dan zip indirilirse
   klasör "<repo>-main" diye açılır; script o durumda durur ve yeniden adlandırmayı söyler (kendi içinden
   çalıştığı için kendisi adlandıramaz).
2. Proje kökü = Engine'in bir üst klasörü. Orada Project.json varsa proje zaten kurulmuştur; hiçbir şey yapmaz.
3. Proje adını terminalde sorar (varsayılan: kök klasörün adı). Ad CMake hedefi ve exe adı olur.
4. Tools/ProjectTemplate/ içindeki her dosyayı aynı göreli yolla proje köküne kopyalar:
   - Dosyalardaki @PROJECT_NAME@ proje adıyla değişir (şu an CMakeLists.txt ve Project.json'da).
   - "gitignore" / "gitattributes" hedefte ".gitignore" / ".gitattributes" olur. Şablonda noktasız durmalarının
     sebebi: noktalı olsalar Engine reposunun kendi git ayarlarına karışırlardı.
   - Hedefte aynı adlı dosya varsa ona dokunmaz, "Kept existing" diye listeler.
5. Build için gerekli araçları denetler (Python, CMake, Visual Studio C++ araçları, Ninja) ve eksikleri yazar.
6. Sonraki komutları yazar. Git'e dokunmaz (submodule bağlamak için Docs/ProjectSetup.md → Git).

Değiştirmek isterseniz
----------------------
- Yeni projeye bir dosya eklemek/çıkarmak: Tools/ProjectTemplate/'e dosyayı koyun/silin; bu script'te değişiklik
  gerekmez (klasörün tamamı kopyalanır). Proje adı geçecekse dosyaya @PROJECT_NAME@ yazın.
- Noktalı adla yazılması gereken yeni bir dosya: RENAMES sözlüğüne ekleyin.
- Ad kuralı: NAME_PATTERN. Araç sürümleri: MIN_PYTHON, MIN_CMAKE.
- Bu dosya sadece Python standart kütüphanesini kullanır ve Tools/'taki modülleri import etmez; tek başına okunur.
  Testleri: Tools/Tests/test_engine_setup.py (Tools içinde `python -m unittest discover -s Tests`).
"""

import argparse
import os
import re
import shutil
import subprocess
import sys
from dataclasses import dataclass, field
from pathlib import Path

# --- Ayarlar -----------------------------------------------------------------------------------------------------

ENGINE = Path(__file__).resolve().parent  # Bu dosyanın bulunduğu Engine klasörü
ENGINE_FOLDER = "Engine"  # Projede Engine klasörünün zorunlu adı (bkz. adım 1)
TEMPLATE = ENGINE / "Tools" / "ProjectTemplate"  # Yeni projeye kopyalanan dosyalar
PROJECT_FILE = "Project.json"  # Varlığı "proje kurulmuş" demektir; Tools/project_config.py de bu adı okur
PLACEHOLDER = "@PROJECT_NAME@"  # Şablon dosyalarında proje adının yeri
RENAMES = {"gitignore": ".gitignore", "gitattributes": ".gitattributes"}  # şablondaki ad → projedeki ad
NAME_PATTERN = re.compile(r"[A-Za-z][A-Za-z0-9_]{0,63}")  # Harfle başlar; harf, rakam, '_'; en çok 64 karakter
FALLBACK_NAME = "MyGame"  # Klasör adından geçerli bir ad çıkmazsa önerilen ad
MIN_PYTHON = (3, 10)  # Engine/Tools'un kullandığı söz dizimi (X | None) için
MIN_CMAKE = (3, 24)  # Engine'in cmake_minimum_required'ı
# Visual Studio'yu bulan Microsoft aracı; her VS kurulumuyla gelir.
VSWHERE = Path(os.environ.get("ProgramFiles(x86)", r"C:\Program Files (x86)")) / "Microsoft Visual Studio" / "Installer" / "vswhere.exe"


@dataclass
class SetupResult:
    """setup_project'in sonucu; yollar proje köküne göredir."""

    root: Path
    created: list[Path] = field(default_factory=list)  # Yazılan (dry-run'da yazılacak) dosyalar
    skipped: list[Path] = field(default_factory=list)  # Zaten var olduğu için dokunulmayan dosyalar


# --- Proje adı ---------------------------------------------------------------------------------------------------


def name_error(name: str) -> str | None:
    """Ad geçersizse nedenini, geçerliyse None döner."""
    if not NAME_PATTERN.fullmatch(name):
        return "use a letter followed by letters, digits or '_' (at most 64 characters, no spaces)"
    return None


def default_name(root: Path) -> str:
    """Kök klasörün adından izin verilmeyen karakterleri atarak önerilen ad ("Space Game" → "SpaceGame")."""
    candidate = re.sub(r"[^A-Za-z0-9_]", "", root.name)
    return candidate if name_error(candidate) is None else FALLBACK_NAME


def ask_name(root: Path) -> str:
    """Terminalde geçerli bir ad girilene kadar sorar; boş Enter önerilen adı seçer."""
    suggestion = default_name(root)
    while True:
        try:
            name = input(f"Project name [{suggestion}]: ").strip() or suggestion
        except EOFError:  # Girdi yok (ör. boş bir pipe)
            sys.exit("\nNo project name given")
        if (error := name_error(name)) is None:
            return name
        print(f"  Invalid name: {error}")


# --- Dosyaları yazmak --------------------------------------------------------------------------------------------


def project_root(engine: Path) -> Path:
    """Adım 1-2: Engine klasörünün adını denetler, proje kökünü (bir üst klasör) döner."""
    if engine.name != ENGINE_FOLDER:
        sys.exit(f"This folder is named '{engine.name}'. Rename it to '{ENGINE_FOLDER}' (projects include it by that "
                 f"name; a GitHub zip extracts as '<repo>-main') and run EngineSetup.py again.")
    return engine.parent


def template_files(template: Path = TEMPLATE) -> list[tuple[Path, Path]]:
    """Şablondaki her dosya için (kaynak dosya, proje köküne göre hedef yol); RENAMES burada uygulanır."""
    files = []
    for source in sorted(template.rglob("*")):
        if source.is_file() and "__pycache__" not in source.parts:
            relative = source.relative_to(template)
            files.append((source, relative.with_name(RENAMES.get(relative.name, relative.name))))
    return files


def setup_project(engine: Path, name: str, dry_run: bool = False) -> SetupResult:
    """Adım 4: şablonu proje köküne yazar. Var olan dosyalara dokunmaz; kurulmuş projeyi reddeder."""
    root = project_root(engine)
    if (error := name_error(name)) is not None:
        sys.exit(f"Invalid project name '{name}': {error}")
    if (root / PROJECT_FILE).exists():
        sys.exit(f"{root / PROJECT_FILE} already exists; this project is already set up")

    result = SetupResult(root)
    for source, relative in template_files():
        destination = root / relative
        if destination.exists():
            result.skipped.append(relative)
            continue
        result.created.append(relative)
        if dry_run:
            continue
        destination.parent.mkdir(parents=True, exist_ok=True)
        # Bayt düzeyinde kopya: satır sonları (LF) ve kodlama şablondaki gibi kalır, sadece ad yerleşir.
        destination.write_bytes(source.read_bytes().replace(PLACEHOLDER.encode(), name.encode()))
    return result


# --- Ortam denetimi ----------------------------------------------------------------------------------------------


def visual_studio() -> Path | None:
    """C++ x64 araçları kurulu en yeni Visual Studio'nun klasörü; yoksa None (Tools/buildenv.py de aynı sorguyu yapar)."""
    if not VSWHERE.exists():
        return None
    query = [str(VSWHERE), "-latest", "-products", "*", "-requires",
             "Microsoft.VisualStudio.Component.VC.Tools.x86.x64", "-property", "installationPath"]
    install = subprocess.run(query, capture_output=True, text=True).stdout.strip()
    return Path(install) if install else None


def environment_problems() -> list[str]:
    """Adım 5: build'den önce eksik araçlar. Boş liste her şeyin hazır olduğunu gösterir; kurulumu engellemez."""
    problems = []
    if sys.version_info < MIN_PYTHON:
        problems.append(f"Python {MIN_PYTHON[0]}.{MIN_PYTHON[1]}+ is required (this is {sys.version.split()[0]})")

    cmake = shutil.which("cmake")
    if cmake is None:
        problems.append(f"CMake {MIN_CMAKE[0]}.{MIN_CMAKE[1]}+ not found on PATH")
    else:
        output = subprocess.run([cmake, "--version"], capture_output=True, text=True).stdout
        found = re.search(r"(\d+)\.(\d+)", output)
        if found and (int(found[1]), int(found[2])) < MIN_CMAKE:
            problems.append(f"CMake {found[0]} is too old; {MIN_CMAKE[0]}.{MIN_CMAKE[1]}+ is required")

    if sys.platform == "win32":
        install = visual_studio()
        if install is None:
            problems.append("Visual Studio 2022 (Build Tools) with the C++ workload not found")
        # Ninja, VS'nin "C++ CMake tools for Windows" bileşeniyle bu klasöre gelir; p.py MSVC ortamından onu kullanır.
        elif not (install / "Common7" / "IDE" / "CommonExtensions" / "Microsoft" / "CMake" / "Ninja").is_dir():
            problems.append("Ninja not found; add the 'C++ CMake tools for Windows' Visual Studio component")
    return problems


# --- Giriş noktası -----------------------------------------------------------------------------------------------


def main(engine: Path = ENGINE) -> None:
    parser = argparse.ArgumentParser(description="Set up the folder above Engine/ as a new project.")
    parser.add_argument("--dry-run", action="store_true", help="List what would be created without writing")
    args = parser.parse_args()

    # Adı sormadan önce erken çıkış: yanlış klasör adı ya da zaten kurulmuş proje.
    root = project_root(engine)
    if (root / PROJECT_FILE).exists():
        sys.exit(f"{root / PROJECT_FILE} already exists; this project is already set up")
    print(f"Project folder: {root}")
    name = ask_name(root)
    result = setup_project(engine, name, args.dry_run)

    verb = "Would create" if args.dry_run else "Created"
    for relative in result.created:
        print(f"  {verb} {relative.as_posix()}")
    for relative in result.skipped:
        print(f"  Kept existing {relative.as_posix()}")
    if args.dry_run:
        return

    problems = environment_problems()
    if problems:
        print("\nInstall before building:")
        for problem in problems:
            print(f"  - {problem}")
    print(f"""
'{name}' is ready. From {root}:
  python p.py          build and run (an empty Game/ shows the engine starter screen)
  python editor.py     EngineEditor with this project's previews
  python p.py --test   headless tests
First game: Engine/Docs/GettingStarted.md, previews: Engine/Docs/Editor/GettingStarted.md""")


if __name__ == "__main__":
    main()

"""`new`: Engine'in üst klasörünü yeni bir oyun projesi olarak kurar.

Kullanım: proje klasörüne sadece Engine'i koyun (klasörün adı tam olarak "Engine"), sonra proje klasöründen
`python Engine new [--name AD] [--dry-run]`. Ad verilmezse terminalde sorulur. Launcher aynı komutu `--name` ile
soru sormadan çağırır.

Ne yapar (sırayla)
1. Klasör adını denetler. Proje CMake'i `add_subdirectory(Engine)` der ve komutlar `python Engine` ile çalışır; bu
   yüzden ad "Engine" olmak zorunda. GitHub'dan zip indirilirse klasör "<repo>-main" diye açılır; komut o durumda
   durur ve yeniden adlandırmayı söyler.
2. Proje kökünde Project.json varsa proje zaten kurulmuştur; hiçbir şey yapmaz.
3. Tools/ProjectTemplate/ içindeki her dosyayı aynı göreli yolla proje köküne kopyalar:
   - Dosyalardaki @PROJECT_NAME@ proje adıyla değişir.
   - "gitignore" / "gitattributes" hedefte ".gitignore" / ".gitattributes" olur; şablonda noktasız durmalarının
     sebebi noktalı olsalar engine reposunun kendi git ayarlarına karışmalarıdır.
   - Hedefte aynı adlı dosya varsa ona dokunmaz, "Kept existing" diye listeler.
4. Build için gerekli araçları denetler (Python, CMake, Visual Studio C++ araçları, Ninja) ve eksikleri yazar.
   Git'e dokunmaz.

Yeni projeye dosya eklemek / çıkarmak için Tools/ProjectTemplate/'i değiştirmek yeter; noktalı adla yazılacak yeni
bir dosya RENAMES'e eklenir.
"""

import re
import shutil
import subprocess
import sys
from dataclasses import dataclass, field
from pathlib import Path

from ..errors import EXIT_OK, ToolError
from ..msvc import NINJA_COMPONENT, visual_studio
from ..project import PROJECT_FILE

ENGINE_FOLDER = "Engine"  # Projede engine klasörünün zorunlu adı (bkz. adım 1)
TEMPLATE = Path(__file__).resolve().parents[1] / "ProjectTemplate"
PLACEHOLDER = "@PROJECT_NAME@"
RENAMES = {"gitignore": ".gitignore", "gitattributes": ".gitattributes"}  # şablondaki ad → projedeki ad
NAME_PATTERN = re.compile(r"[A-Za-z][A-Za-z0-9_]{0,63}")  # Harfle başlar; harf, rakam, '_'; en çok 64 karakter
FALLBACK_NAME = "MyGame"  # Klasör adından geçerli bir ad çıkmazsa önerilen ad
MIN_PYTHON = (3, 10)
MIN_CMAKE = (3, 24)  # Engine'in cmake_minimum_required'ı


@dataclass
class SetupResult:
    """setup_project'in sonucu; yollar proje köküne göredir."""

    root: Path
    created: list[Path] = field(default_factory=list)  # Yazılan (dry-run'da yazılacak) dosyalar
    skipped: list[Path] = field(default_factory=list)  # Zaten var olduğu için dokunulmayan dosyalar


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
        except EOFError as error:  # Girdi yok (ör. boş bir pipe)
            raise ToolError("No project name given; pass --name") from error
        if (error := name_error(name)) is None:
            return name
        print(f"  Invalid name: {error}")


def check_target(engine: Path, root: Path) -> None:
    """Adım 1-2: engine klasörünün adı doğru mu, proje zaten kurulmuş mu."""
    if engine.name != ENGINE_FOLDER:
        raise ToolError(f"The engine folder is named '{engine.name}'. Rename it to '{ENGINE_FOLDER}' (projects include "
                        f"it by that name; a GitHub zip extracts as '<repo>-main') and run the command again.")
    if (root / PROJECT_FILE).exists():
        raise ToolError(f"{root / PROJECT_FILE} already exists; this project is already set up")


def template_files(template: Path = TEMPLATE) -> list[tuple[Path, Path]]:
    """Şablondaki her dosya için (kaynak dosya, proje köküne göre hedef yol); RENAMES burada uygulanır."""
    files = []
    for source in sorted(template.rglob("*")):
        if source.is_file() and "__pycache__" not in source.parts:
            relative = source.relative_to(template)
            files.append((source, relative.with_name(RENAMES.get(relative.name, relative.name))))
    return files


def setup_project(engine: Path, root: Path, name: str, dry_run: bool = False) -> SetupResult:
    """Adım 3: şablonu proje köküne yazar. Var olan dosyalara dokunmaz; kurulmuş projeyi reddeder."""
    check_target(engine, root)
    if (error := name_error(name)) is not None:
        raise ToolError(f"Invalid project name '{name}': {error}")
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


def environment_problems() -> list[str]:
    """Adım 4: build'den önce eksik araçlar. Boş liste her şeyin hazır olduğunu gösterir; kurulumu engellemez."""
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
        elif not (install / "Common7" / "IDE" / "CommonExtensions" / "Microsoft" / "CMake" / "Ninja").is_dir():
            problems.append(f"Ninja not found; add the {NINJA_COMPONENT} Visual Studio component")
    return problems


def new(engine: Path, root: Path, name: str | None, dry_run: bool) -> int:
    check_target(engine, root)  # Adı sormadan önce erken çıkış
    print(f"Project folder: {root}")
    name = name if name is not None else ask_name(root)
    result = setup_project(engine, root, name, dry_run)

    verb = "Would create" if dry_run else "Created"
    for relative in result.created:
        print(f"  {verb} {relative.as_posix()}")
    for relative in result.skipped:
        print(f"  Kept existing {relative.as_posix()}")
    if dry_run:
        return EXIT_OK

    problems = environment_problems()
    if problems:
        print("\nInstall before building:")
        for problem in problems:
            print(f"  - {problem}")
    print(f"""
'{name}' is ready. From {root}:
  python Engine run      build and run (an empty Game/ shows the engine starter screen)
  python Engine editor   the engine editor with this project's scenes
  python Engine test     headless tests
First game: Engine/Docs/GettingStarted.md, scenes: Engine/Docs/Editor/GettingStarted.md""")
    print("Using an AI agent: import Engine/AgentDocs/Agent.md from your own agent file "
          "(Claude Code: @Engine/AgentDocs/Agent.md in CLAUDE.md); Engine/Docs/ProjectSetup.md#using-an-ai-agent.")
    return EXIT_OK

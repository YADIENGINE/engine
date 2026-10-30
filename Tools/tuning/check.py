"""`python Engine check` için tuning denetimi: projedeki her `*.tuning.json` geçerli ve kanonik biçimde mi."""

from pathlib import Path

from ..include_boundaries import skipped_directory
from .document import TuningError
from .header import SUFFIX, struct_name
from .layout import canonical


def tuning_files(root: Path):
    for entry in sorted(root.iterdir()):
        if entry.is_dir():
            if not skipped_directory(entry):
                yield from tuning_files(entry)
        elif entry.name.endswith(SUFFIX):
            yield entry


def check_tuning(root: Path, fix: bool) -> tuple[list[str], list[str]]:
    """(sorunlar, düzeltilenler). `fix` geçerli ama kanonik olmayan dosyaları yeniden yazar; geçersiz dosyaya
    dokunulmaz."""
    root = root.resolve()
    problems, fixed = [], []
    for path in tuning_files(root):
        where = path.relative_to(root).as_posix()
        try:
            struct_name(path)
            text = path.read_bytes().decode("utf-8")
            expected = canonical(text)
        except UnicodeDecodeError:
            problems.append(f"{where}:1: the file is not UTF-8")
            continue
        except TuningError as error:
            problems.append(f"{where}:{error.line}: {error.text}")
            continue
        if text.replace("\r\n", "\n") == expected:  # Satır sonu git'in işi (autocrlf); düzeltme LF yazar
            continue
        if fix:
            path.write_text(expected, encoding="utf-8", newline="\n")
            fixed.append(where)
        else:
            problems.append(f"{where}: not in the canonical layout (fix: check --fix)")
    return problems, fixed

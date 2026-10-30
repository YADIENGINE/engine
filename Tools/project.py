"""Proje kökündeki Project.json: komutların okuduğu proje ayarı.

Şema ve alanlar: Docs/ProjectSetup.md. Projeye özel Python işi (ör. paketlenecek asset listesi) `hooks`
alanındaki dosyadadır; araçlar sadece bilinen fonksiyonları çağırır.
"""

import importlib.util
import json
from pathlib import Path
from types import ModuleType

from .errors import ToolError

PROJECT_FILE = "Project.json"
SCHEMA_VERSION = 1


class Project:
    def __init__(self, root: Path):
        self.root = root.resolve()
        path = self.root / PROJECT_FILE
        if not path.is_file():
            raise ToolError(f"{path} not found; see Engine/Docs/ProjectSetup.md")
        try:
            data = json.loads(path.read_text(encoding="utf-8"))
        except ValueError as error:
            raise ToolError(f"{path}: {error}") from error
        if data.get("version") != SCHEMA_VERSION:
            raise ToolError(f"{path}: expected \"version\": {SCHEMA_VERSION}")
        self.name: str = data.get("name", self.root.name)
        self._data = data
        self._hooks: ModuleType | None = None

    def section(self, name: str) -> dict:
        value = self._data.get(name)
        if not isinstance(value, dict):
            raise ToolError(f"{PROJECT_FILE} has no \"{name}\" section")
        return value

    def path(self, relative: str) -> Path:
        return self.root / relative

    @property
    def msvc_cache(self) -> Path:
        """vcvars64 ortamının önbelleği; oyun, test ve editör build'leri paylaşır (bkz. msvc.msvc_env)."""
        return self.root / "build" / ".msvc-env.json"

    def hook(self, name: str):
        """`hooks` dosyasındaki fonksiyon; dosya ya da fonksiyon yoksa None."""
        if self._hooks is None and "hooks" in self._data:
            path = self.path(self._data["hooks"])
            spec = importlib.util.spec_from_file_location("project_hooks", path)
            if spec is None or spec.loader is None or not path.is_file():
                raise ToolError(f"Cannot load project hooks: {path}")
            self._hooks = importlib.util.module_from_spec(spec)
            spec.loader.exec_module(self._hooks)
        return getattr(self._hooks, name, None) if self._hooks is not None else None

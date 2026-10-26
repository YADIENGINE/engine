"""EngineEditor'ü projenin preview worker'ıyla derler ve açar; iş Engine/Tools/editor_cli.py'de.

Kullanım:
    python editor.py              Debug derle + editörü aç
    python editor.py --release    Release derle + editörü aç
    python editor.py --build      Sadece derle, editörü açma
    python editor.py --clean      Yalnız editör build klasörünü temizleyip yeniden derle + aç
"""

import sys
from pathlib import Path

ROOT = Path(__file__).resolve().parent
sys.path.insert(0, str(ROOT / "Engine" / "Tools"))

from editor_cli import main  # noqa: E402

if __name__ == "__main__":
    main(ROOT)

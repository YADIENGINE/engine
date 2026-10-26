"""Projeyi derleyip çalıştırır; iş Engine/Tools/build_cli.py'de, ayarlar Project.json'da.

Kullanım:
    python p.py              Debug derle + çalıştır
    python p.py --release    Release derle, Final/ paketini kur + çalıştır
    python p.py --build      Sadece derle, çalıştırma
    python p.py --clean      build/ klasörünü silip sıfırdan derle
    python p.py --test       Headless testler (--filter, --label, --list)
"""

import sys
from pathlib import Path

ROOT = Path(__file__).resolve().parent
sys.path.insert(0, str(ROOT / "Engine" / "Tools"))

from build_cli import main  # noqa: E402

if __name__ == "__main__":
    main(ROOT)

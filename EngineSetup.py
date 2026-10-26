"""Engine'in bulunduğu klasörü yeni bir proje olarak kurar; iş Tools/project_setup.py'de.

Kullanım: proje klasörüne sadece Engine/'i koyun, Engine/ içinde çalıştırın:
    python EngineSetup.py              Proje adını sorar, proje dosyalarını bir üst klasöre yazar
    python EngineSetup.py --dry-run    Yazmadan ne oluşturacağını listeler
"""

import sys
from pathlib import Path

ENGINE = Path(__file__).resolve().parent
sys.path.insert(0, str(ENGINE / "Tools"))

from project_setup import main  # noqa: E402

if __name__ == "__main__":
    main(ENGINE)

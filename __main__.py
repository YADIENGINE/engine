"""Engine komut satırı. Proje kökünden: `python Engine <komut>` (komutlar: `python Engine --help`).

Komutlar, seçenekler ve çıkış kodları launcher ile sözleşmedir: Docs/ToolsContract.md.
"""

import sys

sys.dont_write_bytecode = True  # Engine klasörüne __pycache__ yazılmaz; kopya engine salt okunur kalır

from Tools.cli import main  # noqa: E402

sys.exit(main(sys.argv[1:]))

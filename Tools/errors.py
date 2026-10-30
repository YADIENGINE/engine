"""Komutların çıkış kodları ve komutu durduran hata."""

EXIT_OK = 0
EXIT_FAILED = 1  # Projenin işi başarısız: derleme, test, release denetimi
EXIT_INFRA = 2  # Kullanım ya da altyapı: yanlış argüman, eksik araç, bozuk Project.json


class ToolError(Exception):
    """Komutu durduran hata. `cli.main` mesajı yazar ve `exit_code` ile çıkar."""

    def __init__(self, message: str, exit_code: int = EXIT_INFRA):
        super().__init__(message)
        self.exit_code = exit_code

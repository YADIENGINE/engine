"""One build at a time per build folder, across processes.

Two commands that build the same folder at once (an agent starting two `sim run` together, the editor's refresh and a
`build`) make MSVC fail on a locked file, such as `C1041` for the program database. A command takes the folder's lock
before it configures or builds and keeps it until the build ends; a second one waits and says so. The lock file lives
in the system temp folder, keyed by the build folder, so a clean build may delete the folder itself. The operating
system releases the lock when a process dies.
"""

import contextlib
import hashlib
import os
import sys
import tempfile
import time
from pathlib import Path

POLL_SECONDS = 0.2


def lock_path(build_dir: Path) -> Path:
    key = hashlib.sha1(os.path.normcase(str(build_dir.resolve())).encode("utf-8")).hexdigest()[:16]
    return Path(tempfile.gettempdir()) / "engine-build-locks" / f"{key}.lock"


def _try_lock(handle) -> bool:
    try:
        if sys.platform == "win32":
            import msvcrt

            handle.seek(0)
            msvcrt.locking(handle.fileno(), msvcrt.LK_NBLCK, 1)
        else:
            import fcntl

            fcntl.flock(handle.fileno(), fcntl.LOCK_EX | fcntl.LOCK_NB)
        return True
    except OSError:
        return False


def _unlock(handle) -> None:
    if sys.platform == "win32":
        import msvcrt

        handle.seek(0)
        msvcrt.locking(handle.fileno(), msvcrt.LK_UNLCK, 1)
    else:
        import fcntl

        fcntl.flock(handle.fileno(), fcntl.LOCK_UN)


@contextlib.contextmanager
def build_lock(build_dir: Path):
    """Holds the build folder's lock for the `with` block; waits while another process holds it."""
    path = lock_path(build_dir)
    path.parent.mkdir(parents=True, exist_ok=True)
    with open(path, "a+b") as handle:
        waited = False
        while not _try_lock(handle):
            if not waited:
                print(f"Waiting for another build of {build_dir} to finish ...", file=sys.stderr, flush=True)
                waited = True
            time.sleep(POLL_SECONDS)
        try:
            yield
        finally:
            _unlock(handle)

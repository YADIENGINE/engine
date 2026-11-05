"""One build at a time per build folder: a second taker waits, other folders do not."""

import contextlib
import io
import tempfile
import threading
import time
import unittest
from pathlib import Path

from Tools.build_lock import _try_lock, _unlock, build_lock, lock_path


class BuildLockTests(unittest.TestCase):
    def setUp(self):
        self.temp = tempfile.TemporaryDirectory()
        self.addCleanup(self.temp.cleanup)
        self.build = Path(self.temp.name) / "build" / "simulation"

    def held_elsewhere(self, build: Path) -> bool:
        path = lock_path(build)
        path.parent.mkdir(parents=True, exist_ok=True)
        with open(path, "a+b") as handle:
            if _try_lock(handle):
                _unlock(handle)
                return False
            return True

    def test_lock_is_held_inside_the_block_only(self):
        with build_lock(self.build):
            self.assertTrue(self.held_elsewhere(self.build))
            self.assertFalse(self.held_elsewhere(self.build.parent / "game"))
            self.assertFalse(self.build.exists())  # The lock file is not in the folder a clean build removes
        self.assertFalse(self.held_elsewhere(self.build))

    def test_a_second_taker_waits_and_says_so(self):
        order = []
        released = threading.Event()

        def first():
            with build_lock(self.build):
                order.append("first in")
                released.wait(5)
                time.sleep(0.3)
                order.append("first out")

        thread = threading.Thread(target=first)
        thread.start()
        while not order:
            time.sleep(0.01)
        released.set()
        message = io.StringIO()
        with contextlib.redirect_stderr(message), build_lock(self.build):
            order.append("second in")
        thread.join()
        self.assertEqual(order, ["first in", "first out", "second in"])
        self.assertIn("Waiting for another build of", message.getvalue())


if __name__ == "__main__":
    unittest.main()

import importlib.util
from pathlib import Path
import tempfile
import unittest
from unittest.mock import patch

spec = importlib.util.spec_from_file_location("minimap_preflight", Path(__file__).parents[1] / "tools/minimap_preflight.py")
preflight = importlib.util.module_from_spec(spec)
spec.loader.exec_module(preflight)


class PreflightTest(unittest.TestCase):
    def test_states(self):
        with tempfile.TemporaryDirectory() as directory:
            root = Path(directory)
            self.assertEqual(preflight.inspect(root)["status"], "ABSENT")
            self.assertEqual(preflight.inspect(root, False)["status"], "DISABLED")
            expected = {}
            for index, name in enumerate(preflight.EXPECTED):
                path = root / name
                path.parent.mkdir(parents=True, exist_ok=True)
                path.write_bytes(str(index).encode())
                expected[name] = preflight.hashlib.sha256(path.read_bytes()).hexdigest()
                if index == 0:
                    self.assertEqual(preflight.inspect(root, False)["status"], "PARTIAL")
            with patch.object(preflight, "EXPECTED", expected):
                self.assertEqual(preflight.inspect(root)["status"], "EXACT")
                self.assertEqual(preflight.inspect(root, False)["status"], "DISABLED")
                (root / next(iter(expected))).write_bytes(b"wrong")
                self.assertEqual(preflight.inspect(root)["status"], "MISMATCHED")
                self.assertFalse(preflight.inspect(root, False)["launch_allowed"])


if __name__ == "__main__":
    unittest.main()

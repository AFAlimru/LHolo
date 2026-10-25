import hashlib
import importlib.util
import json
from pathlib import Path
import tempfile
import unittest
import zipfile

SCRIPT = Path(__file__).resolve().parents[1] / "prepare_runtime.py"
spec = importlib.util.spec_from_file_location("lholo_runtime_source", SCRIPT)
runtime = importlib.util.module_from_spec(spec)
spec.loader.exec_module(runtime)


class RuntimeSourceTests(unittest.TestCase):
    def setUp(self):
        self.temp = tempfile.TemporaryDirectory()
        self.addCleanup(self.temp.cleanup)
        self.root = Path(self.temp.name)
        self.output = self.root / "output"
        self.archive = self.root / "runtime.zip"
        self.files = {"gameplay.html": b"html fixture", "gameplay-test.js": b"script fixture"}
        with zipfile.ZipFile(self.archive, "w") as package:
            for name, content in self.files.items():
                package.writestr(name, content)
        self.manifest = {"archive": self.archive.name,
            "archiveSha256": hashlib.sha256(self.archive.read_bytes()).hexdigest(),
            "files": {name: hashlib.sha256(data).hexdigest() for name, data in self.files.items()}}
        self.path = self.root / "source.json"

    def prepare(self):
        self.path.write_text(json.dumps(self.manifest))
        runtime.prepare_runtime(self.path, self.output)

    def test_verified_inputs_and_repeat_preparation(self):
        self.prepare()
        before = {name: (self.output / name).stat().st_mtime_ns for name in self.files}
        self.prepare()
        for name, content in self.files.items():
            self.assertEqual((self.output / name).read_bytes(), content)
            self.assertEqual((self.output / name).stat().st_mtime_ns, before[name])

    def test_corrupt_archive_rejected_before_writes(self):
        self.archive.write_bytes(self.archive.read_bytes() + b"changed")
        with self.assertRaisesRegex(ValueError, "archive SHA256 mismatch"):
            self.prepare()
        self.assertFalse(self.output.exists())

    def test_corrupt_member_digest_rejected_before_writes(self):
        self.manifest["files"]["gameplay-test.js"] = "0" * 64
        with self.assertRaisesRegex(ValueError, "input SHA256 mismatch"):
            self.prepare()
        self.assertFalse(self.output.exists())

    def test_unexpected_inventory_rejected_before_writes(self):
        del self.manifest["files"]["gameplay-test.js"]
        with self.assertRaisesRegex(ValueError, "inventory mismatch"):
            self.prepare()
        self.assertFalse(self.output.exists())

    def test_repository_input_matches_adapter(self):
        runtime.prepare_runtime(runtime.SOURCE, self.output)
        adapter_script = SCRIPT.with_name("build_menu.py").read_text(encoding="utf-8")
        manifest = json.loads(runtime.SOURCE.read_text(encoding="utf-8"))
        script_hash = next(digest for name, digest in manifest["files"].items() if name.endswith(".js"))
        self.assertIn(script_hash, adapter_script)


if __name__ == "__main__":
    unittest.main()

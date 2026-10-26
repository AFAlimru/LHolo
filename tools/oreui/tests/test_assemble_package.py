"""Package contract tests, with no game install or native compiler required."""
import contextlib
import importlib.util
import io
import json
from pathlib import Path
import tempfile
import unittest
from unittest.mock import patch

SCRIPT = Path(__file__).resolve().parents[1] / "assemble_package.py"
spec = importlib.util.spec_from_file_location("lholo_package", SCRIPT)
package = importlib.util.module_from_spec(spec)
spec.loader.exec_module(package)


class PackageTests(unittest.TestCase):
    def setUp(self):
        self.temp = tempfile.TemporaryDirectory()
        self.addCleanup(self.temp.cleanup)
        self.root = Path(self.temp.name)
        self.native = self.root / "bin/LHolo"
        self.assets = self.root / "build/oreui"
        self.native.mkdir(parents=True)
        self.assets.mkdir(parents=True)
        manifest = {"name": "LHolo", "entry": "LHolo.dll", "type": "native",
                    "platform": "client", "version": "26.51.5"}
        (self.native / "manifest.json").write_text(json.dumps(manifest))
        (self.root / "tooth.json").write_text(json.dumps({"version": "26.51.5"}))
        (self.native / "LHolo.dll").write_bytes(b"native test fixture")
        (self.native / "LICENSE").write_text("license fixture")
        (self.native / "LHolo.pdb").write_bytes(b"debug symbols excluded")
        for name in ("lholo-menu.html", "lholo-menu.js"):
            (self.assets / name).write_text(package.MARKER)
        (self.assets / "build-info.json").write_text('{}')

    def run_package(self):
        with patch.object(package, "PROJECT", self.root), contextlib.redirect_stdout(io.StringIO()):
            package.main()

    def test_complete_directory_without_archive(self):
        import hashlib
        self.run_package()
        report = json.loads((self.assets / "package-validation.json").read_text())
        self.assertEqual(len(report["files"]), 6)
        for name, expected_hash in report["files"].items():
            content = (self.root / "bin" / name).read_bytes()
            self.assertEqual(hashlib.sha256(content).hexdigest(), expected_hash)
        self.assertFalse(list((self.root / "bin").glob("*.zip")))
        self.assertEqual(list((self.root / "build").iterdir()), [self.assets])
        self.assertTrue((self.native / "LHolo.pdb").is_file())
        manifest_path = self.native / "resource_packs/LHoloUI/manifest.json"
        identity = json.loads(manifest_path.read_text())["header"]["uuid"]
        self.run_package()
        self.assertEqual(identity, json.loads(manifest_path.read_text())["header"]["uuid"])

    def test_version_mismatch_does_not_publish(self):
        (self.root / "tooth.json").write_text('{"version":"99.0.0"}')
        with self.assertRaisesRegex(ValueError, "versions differ"):
            self.run_package()
        self.assertFalse((self.native / "resource_packs").exists())
        self.assertEqual(list((self.root / "build").iterdir()), [self.assets])

    def test_invalid_assets_do_not_publish(self):
        (self.assets / "lholo-menu.js").write_text("incompatible")
        with self.assertRaisesRegex(ValueError, "protocol marker"):
            self.run_package()
        self.assertFalse((self.native / "resource_packs").exists())


if __name__ == "__main__":
    unittest.main()

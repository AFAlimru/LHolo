"""Assemble and validate the complete bin/LHolo installation directory.

Invoked by the default xmake build after the native modpacker. No ZIP or deploy.
The resource pack is a required part of the installation and release package.
"""
import hashlib
import json
from pathlib import Path
import re
import uuid

PROJECT = Path(__file__).resolve().parents[2]
MARKER = "lholo-menu-v7-settings"


def main():
    native = PROJECT / "bin/LHolo"
    assets = PROJECT / "build/oreui"
    manifest = json.loads((native / "manifest.json").read_text(encoding="utf-8-sig"))
    if any(manifest.get(key) != expected for key, expected in {
        "name": "LHolo", "entry": "LHolo.dll", "type": "native", "platform": "client"
    }.items()):
        raise ValueError("Expected an LHolo native client manifest")
    version = manifest["version"]
    if not re.fullmatch(r"\d+\.\d+\.\d+", version):
        raise ValueError("Native version must have three integer components")
    tooth = json.loads((PROJECT / "tooth.json").read_text(encoding="utf-8-sig"))
    if tooth["version"] != version:
        raise ValueError("Native and tooth versions differ")
    pack = json.loads(Path(__file__).with_name("resource_pack.json").read_text(encoding="utf-8"))
    identities = [pack["header"]["uuid"], pack["modules"][0]["uuid"]]
    if len(set(identities)) != 2:
        raise ValueError("Pack and module UUIDs must differ")
    for identity in identities:
        uuid.UUID(identity)
    for section in (pack["header"], pack["modules"][0]):
        section["version"] = [int(part) for part in version.split(".")]
    files = {f"LHolo/{name}": (native / name).read_bytes()
             for name in ("LHolo.dll", "manifest.json", "LICENSE")}
    files["LHolo/resource_packs/LHoloUI/manifest.json"] = (
        json.dumps(pack, ensure_ascii=False, indent=2) + "\n").encode("utf-8")
    for name in ("lholo-menu.html", "lholo-menu.js"):
        content = (assets / name).read_bytes()
        if MARKER not in content.decode("utf-8"):
            raise ValueError(f"Missing UI protocol marker: {name}")
        files[f"LHolo/resource_packs/LHoloUI/hbui/{name}"] = content
    source = json.loads((assets / "build-info.json").read_text(encoding="utf-8"))
    # Write only the owned installation inventory. Never delete the mod
    # directory or copy config/debug/intermediate files into the package.
    output_root = PROJECT / "bin"
    for relative, content in files.items():
        destination = output_root / relative
        destination.parent.mkdir(parents=True, exist_ok=True)
        destination.write_bytes(content)
    for relative, content in files.items():
        if (output_root / relative).read_bytes() != content:
            raise ValueError(f"Assembled directory mismatch: {relative}")
    report = {
        "version": version, "protocol": MARKER,
        "source": source,
        "files": {name: hashlib.sha256(content).hexdigest() for name, content in files.items()},
    }
    report_path = assets / "package-validation.json"
    report_path.write_text(json.dumps(report, indent=2) + "\n", encoding="utf-8")
    print(f"Complete mod directory: {native}")
    print(f"Verified {len(files)} installation files; no ZIP generated.")


if __name__ == "__main__":
    main()

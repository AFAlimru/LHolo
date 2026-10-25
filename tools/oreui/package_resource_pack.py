"""Build a LOCAL-ONLY resource-pack experiment from already generated menu assets.

Does not modify the game or the standard release directory. The hbui path is a
candidate for native resource resolution, not a claim of game validation.
"""
import hashlib
import json
from pathlib import Path
import re
import uuid
import zipfile

PROJECT = Path(__file__).resolve().parents[2]
MARKER = "lholo-menu-v7-settings"


def main():
    native = PROJECT / "bin/LHolo"
    assets = PROJECT / "build/oreui"
    manifest = json.loads((native / "manifest.json").read_text(encoding="utf-8-sig"))
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
    output = PROJECT / "build/oreui-resource-pack-test"
    # Write an exact ZIP inventory; stale staging files cannot enter this artifact.
    output.mkdir(parents=True, exist_ok=True)
    archive = output / f"LHolo-{version}-resource-pack-local-test.zip"
    with zipfile.ZipFile(archive, "w", zipfile.ZIP_DEFLATED) as target:
        for name, content in files.items():
            target.writestr(name, content)
    with zipfile.ZipFile(archive) as target:
        if target.testzip() is not None or set(target.namelist()) != set(files):
            raise ValueError("ZIP inventory or CRC validation failed")
        for name, content in files.items():
            if target.read(name) != content:
                raise ValueError(f"ZIP round-trip mismatch: {name}")
    report = {
        "localOnly": True, "gameValidated": False, "version": version,
        "candidateResourcePath": "hbui", "protocol": MARKER,
        "source": json.loads((assets / "build-info.json").read_text(encoding="utf-8")),
        "archiveSha256": hashlib.sha256(archive.read_bytes()).hexdigest(),
        "files": {name: hashlib.sha256(content).hexdigest() for name, content in files.items()},
    }
    (output / "validation.json").write_text(json.dumps(report, indent=2) + "\n", encoding="utf-8")
    print(f"LOCAL ONLY: {archive}")
    print(f"Verified {len(files)} ZIP entries, UUIDs, versions, protocol markers and SHA256")
    print("Native pack mounting / hbui resolution: awaiting user game test")


if __name__ == "__main__":
    main()

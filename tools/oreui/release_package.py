"""Archive or verify the complete installation directory for a GitHub release.

Daily xmake builds still produce bin/LHolo only. Release archives include the
same eight installation files, never config, debug symbols or build intermediates.
"""
import argparse
import hashlib
import json
from pathlib import Path
import zipfile

PROJECT = Path(__file__).resolve().parents[2]
FILES = (
    "LHolo/LHolo.dll", "LHolo/manifest.json", "LHolo/LICENSE",
    "LHolo/resource_packs/LHoloUI/manifest.json",
    "LHolo/resource_packs/LHoloUI/ui/_ui_defs.json",
    "LHolo/resource_packs/LHoloUI/ui/lholo_hud_editor.json",
    "LHolo/resource_packs/LHoloUI/hbui/lholo-menu.html",
    "LHolo/resource_packs/LHoloUI/hbui/lholo-menu.js",
)
MARKER = b"lholo-menu-v7-settings"


def validate_files(files, version):
    if set(files) != set(FILES):
        raise ValueError("Release must contain exactly the eight LHolo installation files")
    if any(not content for content in files.values()):
        raise ValueError("Empty installation file")
    manifest = json.loads(files["LHolo/manifest.json"].decode("utf-8-sig"))
    expected = {"name": "LHolo", "entry": "LHolo.dll", "version": version,
                "type": "native", "platform": "client"}
    if any(manifest.get(key) != value for key, value in expected.items()):
        raise ValueError("Release native manifest mismatch")
    pack = json.loads(files["LHolo/resource_packs/LHoloUI/manifest.json"])
    template = json.loads((PROJECT / "tools/oreui/resource_pack.json").read_text(encoding="utf-8"))
    if pack["format_version"] != template["format_version"] or pack["header"]["min_engine_version"] != template["header"]["min_engine_version"]:
        raise ValueError("Release resource pack format/engine mismatch")
    if len(pack["modules"]) != 1:
        raise ValueError("Unexpected resource pack modules")
    for section, original in ((pack["header"], template["header"]),
                              (pack["modules"][0], template["modules"][0])):
        if section["uuid"] != original["uuid"] or section["version"] != list(map(int, version.split("."))):
            raise ValueError("Release resource pack identity/version mismatch")
    if pack["modules"][0]["type"] != "resources":
        raise ValueError("Expected a resource pack module")
    for name in FILES[-2:]:
        if MARKER not in files[name]:
            raise ValueError("Release UI protocol mismatch")


def checksum_path(archive):
    return archive.with_name(archive.name + ".sha256")


def verify_archive(archive, version):
    expected = checksum_path(archive).read_text(encoding="ascii").strip()
    actual = hashlib.sha256(archive.read_bytes()).hexdigest()
    if expected != f"{actual}  {archive.name}":
        raise ValueError("Release archive SHA256 mismatch")
    with zipfile.ZipFile(archive) as package:
        if len(package.namelist()) != len(FILES) or set(package.namelist()) != set(FILES):
            raise ValueError("Release archive contains missing, extra or duplicate files")
        validate_files({name: package.read(name) for name in FILES}, version)


def create_archive(archive, version):
    report = json.loads((PROJECT / "build/oreui/package-validation.json").read_text(encoding="utf-8"))
    files = {name: (PROJECT / "bin" / name).read_bytes() for name in FILES}
    validate_files(files, version)
    if report["version"] != version or set(report["files"]) != set(FILES):
        raise ValueError("Run xmake for the release version first")
    for name, content in files.items():
        if hashlib.sha256(content).hexdigest() != report["files"][name]:
            raise ValueError(f"Outdated installation file: {name}; run xmake first")
    archive.parent.mkdir(parents=True, exist_ok=True)
    with zipfile.ZipFile(archive, "w", compression=zipfile.ZIP_DEFLATED) as package:
        for name, content in files.items():
            package.writestr(name, content)
    digest = hashlib.sha256(archive.read_bytes()).hexdigest()
    checksum_path(archive).write_text(f"{digest}  {archive.name}\n", encoding="ascii")
    verify_archive(archive, version)


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    mode = parser.add_mutually_exclusive_group(required=True)
    mode.add_argument("--output", type=Path)
    mode.add_argument("--verify", type=Path)
    parser.add_argument("--tag", help="Expected release tag, for example v26.51.6")
    args = parser.parse_args()
    version = json.loads((PROJECT / "tooth.json").read_text(encoding="utf-8"))["version"]
    if args.tag and args.tag != "v" + version:
        raise ValueError("Release tag and tooth version differ")
    if args.output:
        create_archive(args.output, version)
        print(f"Created and verified release package: {args.output}")
    else:
        verify_archive(args.verify, version)
        print(f"Verified complete LHolo {version} release: {args.verify}")


if __name__ == "__main__":
    main()

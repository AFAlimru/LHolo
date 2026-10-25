"""Prepare the reviewed, versioned OreUI build inputs without a game install."""
import argparse
import hashlib
import json
from pathlib import Path
import zipfile

SOURCE = Path(__file__).with_name("runtime") / "source.json"


def prepare_runtime(manifest_path, output):
    manifest = json.loads(manifest_path.read_text(encoding="utf-8"))
    archive = manifest_path.parent / manifest["archive"]
    if hashlib.sha256(archive.read_bytes()).hexdigest() != manifest["archiveSha256"]:
        raise ValueError("OreUI runtime archive SHA256 mismatch")
    with zipfile.ZipFile(archive) as package:
        if (len(package.namelist()) != len(manifest["files"])
                or set(package.namelist()) != set(manifest["files"])):
            raise ValueError("OreUI runtime archive inventory mismatch")
        files = {name: package.read(name) for name in manifest["files"]}
    for name, content in files.items():
        if hashlib.sha256(content).hexdigest() != manifest["files"][name]:
            raise ValueError(f"OreUI runtime input SHA256 mismatch: {name}")
    output.mkdir(parents=True, exist_ok=True)
    for name, content in files.items():
        destination = output / name
        if not destination.is_file() or destination.read_bytes() != content:
            destination.write_bytes(content)


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--output", type=Path, required=True)
    args = parser.parse_args()
    prepare_runtime(SOURCE, args.output)
    print(f"Prepared reviewed OreUI build inputs: {args.output}")


if __name__ == "__main__":
    main()

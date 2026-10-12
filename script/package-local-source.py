#!/usr/bin/env python3
"""Convey matching source and vendored dependencies with a zynamfx test binary."""
import argparse
import hashlib
import io
import json
from pathlib import Path
import subprocess
import tarfile


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("destination", type=Path)
    args = parser.parse_args()
    source = Path(__file__).resolve().parent.parent
    destination = args.destination.resolve()
    if destination.exists():
        parser.error("Destination already exists; choose a new filename")
    for required in ["COPYING", "LICENSE-SCOPE.md", "libs/juce/CMakeLists.txt",
                     "plugin/NeuralAmpModelerCore/CMakeLists.txt",
                     "plugin/AudioDSPTools/CMakeLists.txt"]:
        if not (source / required).is_file():
            parser.error("Missing corresponding source: " + required)
    revision = subprocess.check_output(["git", "rev-parse", "HEAD"], cwd=source, text=True).strip()
    if subprocess.check_output(["git", "status", "--porcelain"], cwd=source, text=True).strip():
        parser.error("Commit the delivered source before packaging it")
    omitted = {".git", "build", "build-ui", "__pycache__", ".cache", ".DS_Store"}
    manifest = {"commit": revision, "files": {}}
    files = []
    for path in sorted(source.rglob("*")):
        relative = path.relative_to(source)
        if any(part in omitted for part in relative.parts):
            continue
        if path.name.startswith(".env") and path.name != ".env.example":
            continue
        if relative.as_posix().startswith(("test/files/", "resources/factory-presets/")):
            continue  # Test fixtures and catalog samples are not conveyed with the public app.
        if not path.is_file():
            continue
        if not path.resolve().is_relative_to(source):
            raise RuntimeError("Source symlink leaves checkout: " + str(relative))
        manifest["files"][relative.as_posix()] = hashlib.sha256(path.read_bytes()).hexdigest()
        files.append((path, relative))
    with tarfile.open(destination, "w:gz", compresslevel=6, dereference=True) as archive:
        for path, relative in files:
            archive.add(path, arcname="zynamfx-source/" + relative.as_posix(), recursive=False)
        data = json.dumps(manifest, indent=2).encode()
        info = tarfile.TarInfo("zynamfx-source/SOURCE-MANIFEST.json")
        info.size = len(data)
        archive.addfile(info, io.BytesIO(data))
    # Check the actual delivered bytes, including every dependency source file.
    with tarfile.open(destination) as archive:
        for name, expected in manifest["files"].items():
            stream = archive.extractfile("zynamfx-source/" + name)
            if stream is None or hashlib.sha256(stream.read()).hexdigest() != expected:
                raise RuntimeError("Source archive verification failed: " + name)
    print(f"Verified {len(files)} source files at commit {revision}: {destination}")


if __name__ == "__main__":
    main()

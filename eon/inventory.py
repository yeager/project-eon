"""Safe, read-only inventory of user-supplied Project Eon game data."""

from __future__ import annotations

import argparse
import hashlib
import json
from dataclasses import asdict, dataclass
from pathlib import Path, PurePosixPath
import zipfile


@dataclass(frozen=True)
class Asset:
    container: str
    name: str
    size: int
    sha256: str
    kind: str


def classify(name: str, data: bytes) -> str:
    suffix = PurePosixPath(name).suffix.lower()
    if data[:2] == b"MZ":
        return "dos-mz-executable"
    # Millennium's DOS files use .EXE names but are flat binaries loaded by
    # MILL.COM through a private interrupt API, rather than MZ executables.
    if suffix == ".exe":
        return "dos-flat-executable"
    if suffix == ".com":
        return "dos-com-program"
    if suffix == ".adf" or len(data) == 901_120:
        return "amiga-adf"
    if suffix in {".st", ".msa", ".stx"}:
        return "atari-st-disk"
    if suffix == ".img" and len(data) in {360 * 1024, 720 * 1024, 1_200 * 1024, 1_440 * 1024}:
        return "dos-floppy-image"
    if suffix in {".voc", ".wav"}:
        return "audio"
    if suffix in {".bin", ".lib", ".drv"}:
        return "game-resource"
    return "unknown"


def _asset(container: str, name: str, data: bytes) -> Asset:
    return Asset(container, name, len(data), hashlib.sha256(data).hexdigest(), classify(name, data))


def scan_zip(path: Path) -> list[Asset]:
    """Scan an archive and one nested archive level without extracting it."""
    assets: list[Asset] = []
    with zipfile.ZipFile(path) as outer:
        for info in outer.infolist():
            if info.is_dir():
                continue
            data = outer.read(info)
            if info.filename.lower().endswith(".zip"):
                from io import BytesIO
                with zipfile.ZipFile(BytesIO(data)) as inner:
                    for child in inner.infolist():
                        if not child.is_dir():
                            child_data = inner.read(child)
                            assets.append(_asset(f"{path.name}!{info.filename}", child.filename, child_data))
            else:
                assets.append(_asset(path.name, info.filename, data))
    return assets


def inventory(root: Path) -> dict:
    archives = sorted(root.glob("*.zip"))
    assets = [asset for archive in archives for asset in scan_zip(archive)]
    return {
        "schema": 1,
        "root": str(root.resolve()),
        "archives": [archive.name for archive in archives],
        "assets": [asdict(asset) for asset in assets],
        "counts": {
            kind: sum(asset.kind == kind for asset in assets)
            for kind in sorted({asset.kind for asset in assets})
        },
    }


def main() -> None:
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("directory", type=Path)
    parser.add_argument("--output", type=Path)
    args = parser.parse_args()
    result = inventory(args.directory)
    encoded = json.dumps(result, indent=2, ensure_ascii=False) + "\n"
    if args.output:
        args.output.write_text(encoded, encoding="utf-8")
    else:
        print(encoded, end="")


if __name__ == "__main__":
    main()

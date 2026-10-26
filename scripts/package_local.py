#!/usr/bin/env python3
"""Add locally owned game files to the bring-up package; never publishes it."""
import argparse
from pathlib import Path
import zipfile


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("game_dir", type=Path)
    args = parser.parse_args()
    repo = Path(__file__).resolve().parent.parent
    source = args.game_dir.resolve(strict=True)
    for required in ("DesertStorm.exe", "FrontEnd.dll", "Training1.dll", "Data"):
        if not (source / required).exists():
            parser.error(f"Missing game input: {source / required}")
    # Installed game payload only: omit GOG installer, support and redistributables.
    extensions = {".exe", ".dll", ".dat", ".sch", ".key", ".pdf"}
    files = sorted(p for p in source.iterdir()
                   if p.is_file() and p.suffix.lower() in extensions
                   and not p.name.lower().startswith("gog"))
    files += sorted(p for p in (source / "Data").rglob("*") if p.is_file())
    for file in files:
        if not file.resolve().is_relative_to(source):
            parser.error(f"Game file points outside the source directory: {file}")
    output = repo / "dist/desert-storm-local.zip"
    temporary = output.with_suffix(".zip.tmp")
    try:
        with zipfile.ZipFile(repo / "dist/desert-storm-bringup.zip") as base, \
                zipfile.ZipFile(temporary, "w", zipfile.ZIP_DEFLATED) as package:
            names = set(base.namelist())
            for file in files:
                name = "desert-storm-bringup/" + file.relative_to(source).as_posix()
                if name in names:
                    raise ValueError(f"Game file conflicts with runtime package: {name}")
                names.add(name)
            for entry in base.infolist():
                package.writestr(entry, base.read(entry))
            for file in files:
                package.write(file, "desert-storm-bringup/" + file.relative_to(source).as_posix())
        temporary.replace(output)
    finally:
        temporary.unlink(missing_ok=True)
    print(f"{output}: added {len(files)} game files beside the native executables.")
    print("Local package contains owned game data. The game runtime is still incomplete.")


if __name__ == "__main__":
    main()

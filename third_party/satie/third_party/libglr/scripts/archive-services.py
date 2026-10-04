#!/usr/bin/env python3
"""Archive generated binding directories for libglr."""

from __future__ import annotations

import argparse
import json
import os
from pathlib import Path
import shutil
import subprocess
import sys
import tarfile
import tempfile
from typing import Iterable

FORMATS = {"gzip", "deb", "rpm", "zlib", "bzip2", "pacman"}
EXTENSIONS = {
    "gzip": ".tar.gz",
    "zlib": ".tar.zz",
    "bzip2": ".tar.bz2",
    "deb": ".deb.tar.gz",
    "rpm": ".rpm.tar.gz",
    "pacman": ".pkg.tar.gz",
}


def parse_args() -> argparse.Namespace:
    parser = argparse.ArgumentParser(
        description="Archive a generated libglr binding directory."
    )
    parser.add_argument("--input", required=True, help="Binding directory to archive")
    parser.add_argument("--language", required=True, help="Binding language name")
    parser.add_argument("--format", required=True, choices=sorted(FORMATS))
    parser.add_argument("--output", required=True, help="Directory where archive is written")
    return parser.parse_args()


def fail(message: str) -> "None":
    print(f"[archive-services] error: {message}", file=sys.stderr)
    raise SystemExit(1)


def ensure_dir(path: Path) -> None:
    path.mkdir(parents=True, exist_ok=True)


def copy_tree(src: Path, dst: Path) -> None:
    if src.is_dir():
        shutil.copytree(src, dst, dirs_exist_ok=True)
    else:
        shutil.copy2(src, dst)


def collect_inventory(root: Path) -> list[str]:
    return sorted(
        str(path.relative_to(root))
        for path in root.rglob("*")
        if path.is_file()
    )


def add_manifest(staging_root: Path, language: str, archive_format: str, source_dir: Path) -> None:
    manifest = {
        "name": staging_root.name,
        "language": language,
        "archive_format": archive_format,
        "source_directory": str(source_dir.resolve()),
        "files": collect_inventory(staging_root),
    }
    (staging_root / "ARCHIVE_MANIFEST.json").write_text(
        json.dumps(manifest, indent=2) + "\n", encoding="utf-8"
    )


def maybe_add_package_metadata(staging_root: Path, language: str, archive_format: str) -> None:
    if archive_format not in {"deb", "rpm", "pacman"}:
        return

    metadata_dir = staging_root / "package-meta"
    ensure_dir(metadata_dir)
    if archive_format == "deb":
        (metadata_dir / "control").write_text(
            "\n".join(
                [
                    f"Package: libglr-{language}-bindings",
                    "Version: 1.0.0",
                    "Section: devel",
                    "Priority: optional",
                    "Architecture: all",
                    "Maintainer: libglr",
                    f"Description: SWIG-generated libglr bindings for {language}",
                    "",
                ]
            ),
            encoding="utf-8",
        )
    elif archive_format == "rpm":
        (metadata_dir / "libglr-bindings.spec").write_text(
            "\n".join(
                [
                    f"Name: libglr-{language}-bindings",
                    "Version: 1.0.0",
                    "Release: 1%{?dist}",
                    "Summary: SWIG-generated libglr bindings",
                    "License: Unknown",
                    "%description",
                    f"Low-level libglr bindings for {language}.",
                    "",
                ]
            ),
            encoding="utf-8",
        )
    else:
        (metadata_dir / ".PKGINFO").write_text(
            "\n".join(
                [
                    "pkgname = libglr-bindings",
                    "pkgver = 1.0.0-1",
                    f"pkgdesc = SWIG-generated libglr bindings for {language}",
                    "arch = any",
                    "",
                ]
            ),
            encoding="utf-8",
        )


def build_archive(staging_root: Path, output_dir: Path, archive_name: str, archive_format: str) -> Path:
    ensure_dir(output_dir)
    destination = output_dir / f"{archive_name}{EXTENSIONS[archive_format]}"
    mode = {
        "gzip": "w:gz",
        "deb": "w:gz",
        "rpm": "w:gz",
        "pacman": "w:gz",
        "bzip2": "w:bz2",
        "zlib": "w",
    }[archive_format]

    with tarfile.open(destination, mode) as tar:
        tar.add(staging_root, arcname=staging_root.name)

    return destination


def main() -> int:
    args = parse_args()
    input_dir = Path(args.input)
    output_dir = Path(args.output)

    if not input_dir.is_dir():
        fail(f"input directory does not exist: {input_dir}")

    archive_name = input_dir.name
    with tempfile.TemporaryDirectory(prefix="libglr-archive-") as tmpdir:
        staging_root = Path(tmpdir) / archive_name
        copy_tree(input_dir, staging_root)
        maybe_add_package_metadata(staging_root, args.language, args.format)
        add_manifest(staging_root, args.language, args.format, input_dir)
        destination = build_archive(staging_root, output_dir, archive_name, args.format)

    print(destination)
    return 0


if __name__ == "__main__":
    raise SystemExit(main())

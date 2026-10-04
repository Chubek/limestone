#!/usr/bin/env python3
"""Create lightweight packaging metadata for generated libglr bindings."""

from __future__ import annotations

import argparse
import json
from pathlib import Path
import sys

SUPPORTED = {
    "python",
    "ruby",
    "go",
    "java",
    "csharp",
    "perl",
    "php",
    "lua",
    "tcl",
    "javascript",
    "node",
    "r",
    "octave",
}


def parse_args() -> argparse.Namespace:
    parser = argparse.ArgumentParser(
        description="Add language-specific package metadata to a generated binding directory."
    )
    parser.add_argument("--language", required=True, help="Binding language name")
    parser.add_argument("--input", required=True, help="Binding directory to augment")
    return parser.parse_args()


def fail(message: str) -> "None":
    print(f"[packaging-service] error: {message}", file=sys.stderr)
    raise SystemExit(1)


def ensure_dir(path: Path) -> None:
    path.mkdir(parents=True, exist_ok=True)


def write(path: Path, content: str) -> None:
    path.write_text(content, encoding="utf-8")


def package_manifest(root: Path, language: str) -> None:
    files = sorted(str(p.relative_to(root)) for p in root.rglob("*") if p.is_file())
    manifest = {
        "language": language,
        "root": str(root.resolve()),
        "files": files,
    }
    write(root / "PACKAGE_MANIFEST.json", json.dumps(manifest, indent=2) + "\n")


def add_python(root: Path) -> None:
    ensure_dir(root / "src")
    if not (root / "MANIFEST.in").exists():
        write(root / "MANIFEST.in", "recursive-include runtime *\nrecursive-include examples *\n")


def add_ruby(root: Path) -> None:
    ensure_dir(root / "lib")
    if not (root / "lib" / "libglr.rb").exists():
        write(root / "lib" / "libglr.rb", "# Entry point for higher-level Ruby wrappers.\n")


def add_go(root: Path) -> None:
    if not (root / "libglr.go").exists():
        write(root / "libglr.go", "package libglr\n\n// Package scaffold for Go wrappers.\n")


def add_java(root: Path) -> None:
    ensure_dir(root / "src" / "main" / "resources")
    if not (root / "build.gradle").exists():
        write(
            root / "build.gradle",
            "plugins { id 'java-library' }\nversion = '1.0.0'\n",
        )


def add_csharp(root: Path) -> None:
    if not (root / "Directory.Build.props").exists():
        write(
            root / "Directory.Build.props",
            "<Project><PropertyGroup><Version>1.0.0</Version></PropertyGroup></Project>\n",
        )


def add_node(root: Path, language: str) -> None:
    if not (root / "package.json").exists():
        write(
            root / "package.json",
            json.dumps(
                {
                    "name": f"libglr-bindings-{language}",
                    "version": "1.0.0",
                    "private": True,
                    "description": "SWIG-generated libglr bindings",
                },
                indent=2,
            )
            + "\n",
        )


def add_generic(root: Path, language: str) -> None:
    write(
        root / "PACKAGE_NOTES.txt",
        f"This directory contains low-level libglr bindings for {language}.\n"
        "Add your language-native build metadata here if you need more than the default scaffold.\n",
    )


def main() -> int:
    args = parse_args()
    language = args.language.lower()
    root = Path(args.input)

    if not root.is_dir():
        fail(f"input directory does not exist: {root}")

    if language == "javascript":
        language = "node"

    if language not in SUPPORTED:
        add_generic(root, language)
        package_manifest(root, language)
        print(root)
        return 0

    actions = {
        "python": add_python,
        "ruby": add_ruby,
        "go": add_go,
        "java": add_java,
        "csharp": add_csharp,
        "node": lambda p: add_node(p, language),
        "perl": lambda p: add_generic(p, language),
        "php": lambda p: add_generic(p, language),
        "lua": lambda p: add_generic(p, language),
        "tcl": lambda p: add_generic(p, language),
        "r": lambda p: add_generic(p, language),
        "octave": lambda p: add_generic(p, language),
    }
    actions[language](root)
    package_manifest(root, language)
    print(root)
    return 0


if __name__ == "__main__":
    raise SystemExit(main())

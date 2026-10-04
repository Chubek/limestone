#!/usr/bin/env python3
"""
Extract C docstrings (/** ... */ comments) from C source files
using tree-sitter-languages, and output as JSON, YAML, or S-Expression.
"""

import argparse
import json
import sys
from pathlib import Path
from dataclasses import dataclass, asdict

try:
    from tree_sitter_languages import get_language, get_parser
except ImportError:
    sys.exit("Install tree-sitter-languages: pip install tree-sitter-languages")

try:
    import yaml
    HAS_YAML = True
except ImportError:
    HAS_YAML = False


@dataclass
class Docstring:
    file: str
    text: str
    start_line: int
    end_line: int
    next_symbol: str | None = None


def extract_docstrings(source: bytes, filepath: str) -> list[Docstring]:
    """Walk the tree-sitter CST and collect /** ... */ comment nodes."""
    language = get_language("c")
    parser = get_parser("c")
    tree = parser.parse(source)

    results: list[Docstring] = []
    root = tree.root_node
    children = root.children or []

    for i, node in enumerate(children):
        if node.type != "comment":
            continue

        text = node.text.decode("utf-8")
        if not text.startswith("/**"):
            continue

        # look ahead for the symbol this docstring is attached to
        next_symbol = None
        for j in range(i + 1, len(children)):
            sibling = children[j]
            if sibling.type == "comment":
                continue
            next_symbol = _symbol_name(sibling)
            break

        results.append(Docstring(
            file=filepath,
            text=text,
            start_line=node.start_point[0] + 1,
            end_line=node.end_point[0] + 1,
            next_symbol=next_symbol,
        ))

    return results


def _symbol_name(node) -> str | None:
    """Best-effort extraction of the declared name from the next sibling node."""
    if node.type == "function_definition":
        decl = node.child_by_field_name("declarator")
        if decl:
            return _find_identifier(decl)
    elif node.type == "declaration":
        decl = node.child_by_field_name("declarator")
        if decl:
            return _find_identifier(decl)
    elif node.type in ("struct_specifier", "enum_specifier", "union_specifier"):
        name = node.child_by_field_name("name")
        if name:
            return name.text.decode("utf-8")
    elif node.type == "type_definition":
        decl = node.child_by_field_name("declarator")
        if decl:
            return _find_identifier(decl)
    return node.type


def _find_identifier(node) -> str | None:
    """Recursively dig for the first identifier in a declarator subtree."""
    if node.type == "identifier":
        return node.text.decode("utf-8")
    for child in (node.children or []):
        result = _find_identifier(child)
        if result:
            return result
    return None


# --------------- output formatters ---------------

def to_json(docs: list[Docstring]) -> str:
    return json.dumps([asdict(d) for d in docs], indent=2, ensure_ascii=False)


def to_yaml(docs: list[Docstring]) -> str:
    if not HAS_YAML:
        sys.exit("YAML output requires PyYAML: pip install pyyaml")
    return yaml.dump(
        [asdict(d) for d in docs],
        default_flow_style=False,
        allow_unicode=True,
        sort_keys=False,
    )


def to_sexp(docs: list[Docstring]) -> str:
    """Simple S-Expression serialization."""
    lines: list[str] = ["(docstrings"]
    for d in docs:
        escaped = d.text.replace("\\", "\\\\").replace('"', '\\"')
        lines.append(
            f'  (docstring\n'
            f'    (file "{d.file}")\n'
            f'    (text "{escaped}")\n'
            f'    (start_line {d.start_line})\n'
            f'    (end_line {d.end_line})\n'
            f'    (next_symbol "{d.next_symbol or ""}"))'
        )
    lines.append(")")
    return "\n".join(lines)


FORMATTERS = {
    "json": to_json,
    "yaml": to_yaml,
    "sexp": to_sexp,
}


def main():
    ap = argparse.ArgumentParser(description="Extract C docstrings via tree-sitter")
    ap.add_argument("files", nargs="+", help="C source files to process")
    ap.add_argument(
        "-f", "--format",
        choices=FORMATTERS,
        default="json",
        help="Output format (default: json)",
    )
    ap.add_argument("-o", "--output", help="Output file (default: stdout)")
    args = ap.parse_args()

    all_docs: list[Docstring] = []
    for path_str in args.files:
        path = Path(path_str)
        if not path.is_file():
            print(f"warning: skipping {path} (not a file)", file=sys.stderr)
            continue
        source = path.read_bytes()
        all_docs.extend(extract_docstrings(source, str(path)))

    output = FORMATTERS[args.format](all_docs)

    if args.output:
        Path(args.output).write_text(output, encoding="utf-8")
    else:
        print(output)


if __name__ == "__main__":
    main()

"""Line-by-line directive / block parser producing a flat node list."""

import re
import shlex
from dataclasses import dataclass, field
from typing import Optional


@dataclass
class ModuleDecl:
    name: str


@dataclass
class VersionDecl:
    version: str


@dataclass
class ParamDecl:
    name: str
    default: Optional[str] = None


@dataclass
class IncludeDecl:
    path: str


@dataclass
class GuardDecl:
    pass


@dataclass
class RequireDecl:
    cond: str
    msg: Optional[str] = None


@dataclass
class ImportDecl:
    module: str
    overrides: dict = field(default_factory=dict)


@dataclass
class BlockNode:
    kind: str  # "struct" | "fn" | "raw"
    name: str  # fn name, or "" for struct/raw
    body: str  # raw template text
    tcl: bool = False


_COMMENT = re.compile(r"\s*#.*$")


def _strip_comment(line: str) -> str:
    return _COMMENT.sub("", line)


def _unquote(text: str) -> str:
    """Strip one pair of matching surrounding quotes, if present."""
    if len(text) >= 2 and text[0] == text[-1] and text[0] in ("'", '"'):
        return text[1:-1]
    return text


def _extract_braced(text: str, lineno: int) -> tuple[str, str]:
    """Split '{...} rest' into (inner, rest), honouring nested braces."""
    if not text.startswith("{"):
        raise SyntaxError(f"Expected '{{...}}' on line {lineno}: {text!r}")
    depth = 0
    for idx, ch in enumerate(text):
        if ch == "{":
            depth += 1
        elif ch == "}":
            depth -= 1
            if depth == 0:
                return text[1:idx], text[idx + 1 :].strip()
    raise SyntaxError(f"Unterminated '{{...}}' on line {lineno}: {text!r}")


def _read_block(lines: list[str], start: int) -> tuple[str, int]:
    """Collect a '{ ... }' block starting at line `start`.

    `start` is the index of the line holding the opening brace (either a
    directive line such as '@fn init {' or a bare '{' line). Returns
    (body, next_line_index) where body is the text between the outermost
    braces. Nested braces are counted so C struct initializers do not
    confuse the parser.
    """
    if start >= len(lines) or "{" not in lines[start]:
        raise SyntaxError(
            f"Expected '{{' to open block (line {start + 1})"
        )
    depth = 0
    buf: list[str] = []
    i = start
    first_line = True
    while i < len(lines):
        line = lines[i]
        # On the opening line, skip everything up to and including the
        # first '{' (the directive text itself is not part of the body).
        j = line.find("{") if first_line else 0
        first_line = False
        while j < len(line):
            ch = line[j]
            if ch == "{":
                depth += 1
                if depth > 1:
                    buf.append(ch)
            elif ch == "}":
                depth -= 1
                if depth == 0:
                    # End of block; anything after the closing brace on
                    # this line is ignored.
                    return "".join(buf), i + 1
                buf.append(ch)
            else:
                if depth >= 1:
                    buf.append(ch)
            j += 1
        if depth >= 1:
            buf.append("\n")
        i += 1
    raise SyntaxError("Unterminated block: missing closing '}'")


def parse(source: str) -> list:
    """Parse a .q source string into a list of declaration/block nodes.

    Handles brace-counting to support multi-line blocks. Comments (# to
    end of line) are stripped from directive lines before processing;
    block bodies are kept verbatim.
    """
    nodes: list = []
    lines = source.splitlines()
    i = 0
    while i < len(lines):
        line = _strip_comment(lines[i]).rstrip()
        stripped = line.strip()

        if not stripped:
            i += 1
            continue

        if not stripped.startswith("@"):
            # Bare text outside a block — ignore (could warn).
            i += 1
            continue

        parts = stripped.split(None, 2)  # [@directive, arg1, rest]
        directive = parts[0].lower()
        lineno = i + 1

        if directive == "@module":
            if len(parts) < 2:
                raise SyntaxError(f"@module needs a name (line {lineno})")
            nodes.append(ModuleDecl(name=parts[1]))
            i += 1

        elif directive == "@version":
            if len(parts) < 2:
                raise SyntaxError(f"@version needs a value (line {lineno})")
            nodes.append(VersionDecl(version=parts[1]))
            i += 1

        elif directive == "@param":
            if len(parts) < 2:
                raise SyntaxError(f"@param needs a name (line {lineno})")
            name = parts[1]
            default = parts[2].strip() if len(parts) > 2 else None
            if default == "":
                default = None
            nodes.append(ParamDecl(name=name, default=default))
            i += 1

        elif directive == "@include":
            rest = stripped[len("@include") :].strip()
            if not rest:
                raise SyntaxError(f"@include needs a path (line {lineno})")
            nodes.append(IncludeDecl(path=rest))
            i += 1

        elif directive == "@guard":
            nodes.append(GuardDecl())
            i += 1

        elif directive == "@require":
            # @require {tcl_expr} "optional message"
            rest = stripped[len("@require") :].strip()
            cond, msg = _extract_braced(rest, lineno)
            msg = _unquote(msg) if msg else None
            nodes.append(RequireDecl(cond=cond, msg=msg))
            i += 1

        elif directive == "@import":
            if len(parts) < 2:
                raise SyntaxError(f"@import needs a module name (line {lineno})")
            module = parts[1]
            overrides: dict = {}
            if len(parts) > 2:
                try:
                    tokens = shlex.split(parts[2])
                except ValueError:
                    tokens = parts[2].split()
                for kv in tokens:
                    k, _, v = kv.partition("=")
                    if not _:
                        raise SyntaxError(
                            f"Bad @import override {kv!r} (line {lineno}): "
                            "expected NAME=VALUE"
                        )
                    overrides[k] = v
            nodes.append(ImportDecl(module=module, overrides=overrides))
            i += 1

        elif directive in ("@struct", "@raw"):
            if "{" in stripped:
                brace_line = i
            else:
                brace_line = i + 1
            body, i = _read_block(lines, brace_line)
            kind = directive[1:]  # "struct" or "raw"
            nodes.append(BlockNode(kind=kind, name="", body=body))

        elif directive == "@fn":
            # @fn NAME [-tcl] {
            fn_parts = stripped.split(None)
            if len(fn_parts) < 2 or fn_parts[1] in ("{", "-tcl"):
                raise SyntaxError(f"@fn needs a name (line {lineno})")
            fn_name = fn_parts[1]
            use_tcl = "-tcl" in fn_parts
            brace_line = i if "{" in stripped else i + 1
            body, i = _read_block(lines, brace_line)
            nodes.append(BlockNode(kind="fn", name=fn_name, body=body, tcl=use_tcl))

        else:
            raise SyntaxError(f"Unknown directive: {directive!r} on line {lineno}")

    return nodes

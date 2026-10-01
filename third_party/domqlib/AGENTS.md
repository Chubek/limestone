# The Q Language

## Overview

Q is a lightweight DSL for authoring type-generic, header-only C libraries. A `.q` file describes one *module* — its parameters, its types, and its functions — using a mix of Q directives and verbatim C template blocks. The compiler instantiates the module for a given set of parameter values and emits a self-contained `.h` file (or an in-memory string).

The runtime is pure Python. `tkinter.Tcl` provides a Tcl interpreter as the substitution engine; Python drives parsing, directive dispatch, and output assembly.

---

## File Structure

A `.q` file is a sequence of **directives** and **C template blocks**. Lines beginning with `@` are directives; everything else inside a block body `{ ... }` is a C template.

```q
# stack.q — generic stack, header-only

@module  stack
@version 1.0

@param T            # required — element type, e.g. int, float, MyStruct*
@param PREFIX stack # optional — naming prefix; defaults to @module name
@param CAP    64    # optional — default capacity constant

@include <stdlib.h>
@include <assert.h>

@guard   # emit include-guard using module name + param fingerprint

@struct {
    typedef struct {
        $T  *data;
        int  top;
        int  cap;
    } ${PREFIX}_t;
}

@fn init {
    static inline void
    ${PREFIX}_init(${PREFIX}_t *s, int cap) {
        s->data = ($T *)malloc(sizeof($T) * (size_t)cap);
        assert(s->data);
        s->cap  = cap;
        s->top  = 0;
    }
}

@fn push {
    static inline void
    ${PREFIX}_push(${PREFIX}_t *s, $T val) {
        assert(s->top < s->cap);
        s->data[s->top++] = val;
    }
}

@fn pop {
    static inline $T
    ${PREFIX}_pop(${PREFIX}_t *s) {
        assert(s->top > 0);
        return s->data[--s->top];
    }
}

@fn peek {
    static inline $T
    ${PREFIX}_peek(const ${PREFIX}_t *s) {
        assert(s->top > 0);
        return s->data[s->top - 1];
    }
}

@fn empty {
    static inline int
    ${PREFIX}_empty(const ${PREFIX}_t *s) {
        return s->top == 0;
    }
}

@fn free {
    static inline void
    ${PREFIX}_free(${PREFIX}_t *s) {
        free(s->data);
        s->data = NULL;
        s->top  = 0;
        s->cap  = 0;
    }
}
```

---

## Directives

| Directive | Purpose |
|---|---|
| `@module NAME` | Declares the module name. Required, must come first. |
| `@version VER` | Optional semver string, embedded in a comment. |
| `@param NAME [DEFAULT]` | Declares a template parameter. Without a default it is required at compile time. |
| `@include PATH` | Emits `#include PATH` verbatim. |
| `@guard` | Emits `#ifndef / #define / #endif` include guards. The guard symbol is derived from the module name and a hash of the bound parameter values, so two instantiations of the same module with different types can coexist in the same translation unit. |
| `@struct { ... }` | C template block for type definitions. |
| `@fn NAME { ... }` | C template block for one function. |
| `@raw { ... }` | Emits the block verbatim after substitution — for macros, constants, or anything that doesn't fit `@struct` / `@fn`. |
| `@import MODULE` | Pulls in another `.q` module (resolved relative to the current file or the search path), compiled with the same parameter bindings unless overridden with `@import MODULE T=$T PREFIX=node`. |
| `@require COND [MSG]` | Compile-time assertion on parameter values. `@require {[string length $T] > 0} "T must be set"`. Evaluated as Tcl. |

---

## Template Variables

Inside any block body, the following substitution rules apply (via Tcl `subst -nocommands`):

- `$NAME` — substitutes parameter `NAME`.
- `${NAME}` — same, but safe when adjacent to identifier characters.
- `${NAME}_suffix` — works naturally because Tcl knows `{}` brackets the name.
- `\$` — a literal `$` (escaped).
- No command substitution (`[...]`) by default. Enable it per-block with `@fn NAME -tcl { ... }` for computed values.

The substitution is intentionally limited — it replaces variables but does not evaluate arbitrary code unless `-tcl` is opted in. This keeps template blocks predictable and safe.

---

## Parameter Rules

- Parameters without defaults are *required*. The compiler raises an error at instantiation time if they are unbound.
- Parameter names are case-sensitive.
- A parameter value is any string. For C types, use `"unsigned long"` (quotes allowed on the command line and Python API).
- `@require` lets you validate values early: `@require {$T ne ""} "T is required"`.

---

## Include Guards

`@guard` generates a symbol like:

```c
#ifndef Q_STACK_4a7f3c_H
#define Q_STACK_4a7f3c_H
/* ... */
#endif /* Q_STACK_4a7f3c_H */
```

The hex fragment is a short hash of the sorted param bindings. This means:

```c
#include "stack_int.h"    /* guard: Q_STACK_9e1a2b_H — T=int  */
#include "stack_float.h"  /* guard: Q_STACK_4a7f3c_H — T=float */
```

Both can be included in the same file without collision.

---

## Python Implementation Architecture
```
q/
  __init__.py       # public API: QCompiler, QError
  compiler.py       # orchestration: parse → bind → emit
  parser.py         # line-by-line directive / block parser → AST
  tcl_engine.py     # thin wrapper around tkinter.Tcl
  directives.py     # directive registry and shared helpers
  guard.py          # include-guard symbol generation
  loader.py         # file resolution, @import graph, cycle detection
  cli.py            # qc entry point
```

### `tcl_engine.py`

```python
import tkinter

class TclEngine:
    """Wraps a tkinter.Tcl interpreter for template substitution."""

    def __init__(self):
        self._tcl = tkinter.Tcl()

    def bind(self, name: str, value: str) -> None:
        """Set a Tcl variable."""
        self._tcl.setvar(name, value)

    def get(self, name: str) -> str:
        """Read back a Tcl variable."""
        return self._tcl.getvar(name)

    def subst(self, template: str, allow_commands: bool = False) -> str:
        """
        Substitute $variables and ${variables} in template.
        By default no command substitution ([...]) is performed.
        The template travels through a Tcl variable so that braces
        in C code cannot break out of the subst call (interpolating
        it into `subst {...}` would terminate at the first `}`).
        """
        self._tcl.setvar("__q_template", template)
        if allow_commands:
            return self._tcl.eval("subst $__q_template")
        return self._tcl.eval("subst -nocommands $__q_template")

    def eval(self, expr: str) -> str:
        """Evaluate a Tcl expression (for @require conditions)."""
        self._tcl.setvar("__q_cond", expr)
        return self._tcl.eval("expr $__q_cond")
```

### `parser.py`

The parser produces a flat list of nodes:

```python
from dataclasses import dataclass, field
from typing import Optional

@dataclass
class ModuleDecl:   name: str
@dataclass
class VersionDecl:  version: str
@dataclass
class ParamDecl:    name: str; default: Optional[str] = None
@dataclass
class IncludeDecl:  path: str
@dataclass
class GuardDecl:    pass
@dataclass
class BlockNode:
    kind: str        # "struct" | "fn" | "raw"
    name: str        # fn name, or "" for struct/raw
    body: str        # raw template text
    tcl:  bool = False

def parse(source: str) -> list:
    """
    Parse a .q source string into a list of declaration/block nodes.
    Handles brace-counting to support multi-line blocks.
    Comments (# to end of line) are stripped before processing.
    """
    ...
```

### `compiler.py`

```python
import tkinter
from pathlib import Path

from .guard import make_guard_symbol
from .loader import CycleError, compilation_guard, resolve_imports
from .parser import (
    BlockNode,
    GuardDecl,
    ImportDecl,
    IncludeDecl,
    ModuleDecl,
    ParamDecl,
    RequireDecl,
    VersionDecl,
    parse,
)
from .tcl_engine import TclEngine


class QError(Exception): pass

class QCompiler:
    def __init__(self, search_path: list[str] | None = None):
        self.search_path = list(search_path) if search_path else ["."]

    def compile(self, source_or_path: str, **params) -> str:
        """
        Compile a .q file or source string with the given parameter bindings.
        Returns the C header as a string.
        """
        origin, source = self._load(source_or_path)
        nodes = parse(source)
        return self._emit(nodes, dict(params), origin)

    @staticmethod
    def _load(source_or_path: str) -> tuple[str, str]:
        """
        Split the input into (origin, source text). Anything naming an
        existing file -- or a single-line string ending in '.q' -- is a
        path (a missing .q file raises FileNotFoundError); everything
        else is inline source. A bare `endswith(".q")` check alone would
        misclassify inline sources, so existence is tested too.
        """
        if "\n" not in source_or_path:
            candidate = Path(source_or_path)
            if source_or_path.strip().endswith(".q") or candidate.exists():
                return str(source_or_path), candidate.read_text()
        return "<string>", source_or_path

    def _emit(self, nodes, params: dict, origin: str) -> str:
        """
        Emit with cycle tracking when compiling a real file. Without this
        guard, `resolve_imports` could never observe an in-progress
        compilation and import cycles would recurse instead of raising.
        """
        if origin != "<string>" and Path(origin).exists():
            with compilation_guard(str(Path(origin).resolve())):
                return self._emit_inner(nodes, params, origin)
        return self._emit_inner(nodes, params, origin)

    def _emit_inner(self, nodes, params: dict, origin: str) -> str:
        engine = TclEngine()
        meta = {}

        # First pass: collect module/param declarations, validate
        for node in nodes:
            if isinstance(node, ModuleDecl):
                meta["module"] = node.name
                if "PREFIX" not in params:
                    params["PREFIX"] = node.name
            elif isinstance(node, VersionDecl):
                meta["version"] = node.version
            elif isinstance(node, ParamDecl):
                if node.name in params:
                    engine.bind(node.name, params[node.name])
                elif node.default is not None:
                    engine.bind(node.name, node.default)
                    params[node.name] = node.default
                else:
                    raise QError(
                        f"Required parameter '{node.name}' "
                        f"not provided for module '{meta.get('module', '?')}'"
                    )

        if "module" not in meta:
            raise QError(f"No @module declaration in {origin}")

        # Bind all params into Tcl
        for k, v in params.items():
            engine.bind(k, v)

        # Header comment
        ver = f" v{meta['version']}" if "version" in meta else ""
        parts = [
            f"/* Generated by Q — {meta['module']}{ver}\n"
            f" * Parameters: "
            + ", ".join(f"{k}={v}" for k, v in sorted(params.items()))
            + "\n */\n"
        ]

        # Second pass: emit
        for node in nodes:
            if isinstance(node, IncludeDecl):
                parts.append(f"#include {node.path}\n")
            elif isinstance(node, GuardDecl):
                if "guard_sym" in meta:
                    continue  # only the first @guard takes effect
                sym = make_guard_symbol(meta["module"], params)
                parts.insert(1, f"#ifndef {sym}\n#define {sym}\n\n")
                # closing #endif appended at the very end
                meta["guard_sym"] = sym
            elif isinstance(node, BlockNode):
                try:
                    substituted = engine.subst(node.body, allow_commands=node.tcl)
                except tkinter.TclError as e:
                    raise QError(
                        f"Substitution failed in '{node.name or node.kind}' "
                        f"(module '{meta['module']}'): {e}"
                    ) from e
                parts.append(substituted.strip() + "\n\n")
            elif isinstance(node, RequireDecl):
                try:
                    result = engine.eval(node.cond)
                except tkinter.TclError as e:
                    raise QError(
                        f"@require could not be evaluated "
                        f"(module '{meta['module']}'): {node.cond}: {e}"
                    ) from e
                if result.strip() != "1":
                    msg = node.msg or f"@require failed: {node.cond}"
                    raise QError(f"{msg} (module '{meta['module']}')")
            elif isinstance(node, ImportDecl):
                # Resolve override values against the current bindings so
                # that e.g. T=$T forwards the caller's value instead of
                # binding the literal string "$T".
                sub_params = dict(params)
                for k, v in node.overrides.items():
                    try:
                        v = engine.subst(v, allow_commands=False)
                    except tkinter.TclError:
                        pass
                    sub_params[k] = v
                search = list(self.search_path)
                if origin != "<string>":
                    parent = str(Path(origin).parent)
                    if parent not in search:
                        search.insert(0, parent)
                try:
                    imported = resolve_imports(node.module, search)
                except CycleError as e:
                    raise QError(str(e)) from e
                try:
                    sub_output = QCompiler(search).compile(imported, **sub_params)
                except CycleError as e:
                    raise QError(str(e)) from e
                parts.append(sub_output.rstrip() + "\n\n")

        if "guard_sym" in meta:
            parts.append(f"#endif /* {meta['guard_sym']} */\n")

        return "".join(parts)
```

---

### `guard.py`

```python
import hashlib
import re

def make_guard_symbol(module: str, params: dict) -> str:
    """
    Derive a collision-resistant include-guard symbol from the module name
    and its bound parameter values.

    e.g. make_guard_symbol("stack", {"T": "int", "PREFIX": "stack"})
         -> "Q_STACK_9e1a2b_H" (hex fragment varies with the bindings)
    """
    # Sort for determinism: same params -> same guard
    fingerprint = ",".join(f"{k}={v}" for k, v in sorted(params.items()))
    digest = hashlib.sha1(fingerprint.encode()).hexdigest()[:6]
    safe_name = re.sub(r"[^A-Z0-9_]", "_", module.upper())
    return f"Q_{safe_name}_{digest}_H"
```

---

### `loader.py`

```python
import contextlib
from pathlib import Path
from typing import Iterator


class CycleError(Exception): pass

_in_progress: set[str] = set()

def resolve_imports(module_name: str, search_path: list[str]) -> str:
    """
    Locate module_name.q on the search path and return its absolute path.
    Raises FileNotFoundError if not found, CycleError if already being compiled.
    """
    filename = f"{module_name}.q"
    for directory in search_path:
        candidate = Path(directory) / filename
        if candidate.exists():
            resolved = str(candidate.resolve())
            if resolved in _in_progress:
                raise CycleError(f"Import cycle detected: {resolved}")
            return resolved
    raise FileNotFoundError(
        f"Cannot find module '{module_name}' on search path {search_path}"
    )

@contextlib.contextmanager
def compilation_guard(path: str) -> Iterator[None]:
    """Track in-progress compilations for cycle detection."""
    resolved = str(Path(path).resolve())
    _in_progress.add(resolved)
    try:
        yield
    finally:
        _in_progress.discard(resolved)
```

---

### `parser.py` (complete)

```python
import re
import shlex
from dataclasses import dataclass, field
from typing import Optional

@dataclass
class ModuleDecl:   name: str
@dataclass
class VersionDecl:  version: str
@dataclass
class ParamDecl:    name: str; default: Optional[str] = None
@dataclass
class IncludeDecl:  path: str
@dataclass
class GuardDecl:    pass
@dataclass
class RequireDecl:  cond: str; msg: Optional[str] = None
@dataclass
class ImportDecl:   module: str; overrides: dict = field(default_factory=dict)
@dataclass
class BlockNode:
    kind: str        # "struct" | "fn" | "raw"
    name: str        # fn name, or "" for struct/raw
    body: str
    tcl:  bool = False

_COMMENT = re.compile(r"\s*#.*$")

def _strip_comment(line: str) -> str:
    return _COMMENT.sub("", line)

def _unquote(text: str) -> str:
    """Strip one pair of matching surrounding quotes, if present."""
    if len(text) >= 2 and text[0] == text[-1] and text[0] in ("'", '"'):
        return text[1:-1]
    return text

def _extract_braced(text: str, lineno: int) -> tuple[str, str]:
    """
    Split '{...} rest' into (inner, rest), honouring nested braces. A
    `\\{([^}]*)\\}` regex cannot do this -- it stops at the first `}`.
    """
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
    """
    Collect a '{ ... }' block starting at line `start` (the line holding
    the opening brace -- a directive line like '@fn init {' or a bare
    '{'). Returns (body, next_line_index): the text between the outermost
    braces. Nested braces are counted so C struct initializers don't
    confuse the parser. Counting whole lines instead of scanning
    characters wrongly keeps the directive line itself in the body.
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
            rest = stripped[len("@include"):].strip()
            if not rest:
                raise SyntaxError(f"@include needs a path (line {lineno})")
            nodes.append(IncludeDecl(path=rest))
            i += 1

        elif directive == "@guard":
            nodes.append(GuardDecl())
            i += 1

        elif directive == "@require":
            # @require {tcl_expr} "optional message"
            rest = stripped[len("@require"):].strip()
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
            # The opening brace is on this line or the next.
            brace_line = i if "{" in stripped else i + 1
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
```

---

### `directives.py`

```python
"""Directive reference for the Q language.

The compiler dispatches directives inline (see compiler.py); this module
documents the directive set and exposes the small helpers shared by the
parser and the compiler.
"""

#: All known directives (without the leading '@').
HANDLER_NAMES = (
    "module",
    "version",
    "param",
    "include",
    "guard",
    "struct",
    "fn",
    "raw",
    "import",
    "require",
)

#: Directives that carry a C template block body.
BLOCK_KINDS = ("struct", "fn", "raw")


def is_block_directive(name: str) -> bool:
    """Return True if `name` (without '@') takes a '{ ... }' body."""
    return name in BLOCK_KINDS
```

---

### `cli.py`

```python
"""
qc — Q language compiler CLI

Usage:
    qc <file.q> [PARAM=VALUE ...] [-o OUTPUT] [-I DIR] [--stdout]

Examples:
    qc stack.q T=int
    qc stack.q T="unsigned long" PREFIX=ulstack -o ulstack.h
    qc mempool.q T=MyStruct CAP=128 --stdout
    qc -I ~/q/lib stack.q T=float
"""

import argparse
import sys
import tkinter
from pathlib import Path

from . import QCompiler, QError
from .loader import CycleError


def main(argv: list[str] | None = None) -> None:
    p = argparse.ArgumentParser(
        prog="qc",
        description="Compile a .q template to a C header."
    )
    p.add_argument("file", help="Path to the .q source file")
    p.add_argument("params", nargs="*", metavar="PARAM=VALUE",
                   help="Parameter bindings, e.g. T=int PREFIX=mystack")
    p.add_argument("-o", "--output", metavar="FILE",
                   help="Write output to FILE (default: <module>_<T>.h)")
    p.add_argument("-I", "--include", action="append", default=[],
                   metavar="DIR", help="Add DIR to the module search path")
    p.add_argument("--stdout", action="store_true",
                   help="Print generated header to stdout")

    args = p.parse_args(argv)

    # Parse PARAM=VALUE pairs
    params: dict = {}
    for pv in args.params:
        if "=" not in pv:
            p.error(f"Expected PARAM=VALUE, got: {pv!r}")
        k, _, v = pv.partition("=")
        params[k] = v

    search_path = [str(Path(args.file).parent)] + args.include

    try:
        compiler = QCompiler(search_path=search_path)
        output = compiler.compile(args.file, **params)
    except QError as e:
        print(f"qc error: {e}", file=sys.stderr)
        sys.exit(1)
    except FileNotFoundError as e:
        print(f"qc: {e}", file=sys.stderr)
        sys.exit(1)
    except (SyntaxError, CycleError, tkinter.TclError) as e:
        print(f"qc error: {e}", file=sys.stderr)
        sys.exit(1)

    if args.output:
        Path(args.output).write_text(output)
    if args.stdout or not args.output:
        sys.stdout.write(output if output.endswith("\n") else output + "\n")
    else:
        print(f"Written: {args.output}")

if __name__ == "__main__":
    main()
```

---

### `__init__.py`

```python
from .compiler import QCompiler, QError

__all__ = ["QCompiler", "QError"]
__version__ = "0.1.0"
```

---

## Usage examples

**CLI:**
```bash
# compile stack.q for int
qc stack.q T=int -o stack_int.h

# compile for unsigned long with a custom prefixt to stdout
qc mempool.q T=MyStruct CAP=128 --stdout

# use a library search path
qc -I ~/q/lib stack.q T=float -o stack_float.h
```
**Python API:**
```python
from q import QCompiler

compiler = QCompiler(search_path=[".", "lib/"])

# returns the header as a string — no file I/O required
header = compiler.compile("stack.q", T="int")

# or inline source
header = compiler.compile("""
@module  vec2
@param   SCALAR float

@struct {
    typedef struct { $SCALAR x, y; } vec2_t;
}

@fn add {
    static inline vec2_t
    vec2_add(vec2_t a, vec2_t b) {
        return (vec2_t){ a.x + b.x, a.y + b.y };
    }
}
""", SCALAR="double")
```
**Including two instantiations in the same C file:**
```c
/* Works because @guard generates distinct symbols per param set */
#include "stack_int.h"    /* Q_STACK_9e1a2b_H */
#include "stack_float.h"  /* Q_STACK_4a7f3c_H */

int main(void) {
    stack_t   si; stack_init(&si, 64);
    floatstack_t sf; floatstack_init(&sf, 64);
    ...
}
```
---

## A second example — `mempool.q`

Shows `@require` and a `@raw` block for macros (`@import` is covered in the Directives table):

```q
# mempool.q — fixed-size arena allocator

@module  mempool
@version 1.0

@param T
@param CAP  256
@param PREFIX mempool

@require {$CAP > 0} "CAP must be positive"

@include <string.h>
@include <assert.h>

@guard

@raw {
    /* pool capacity compiled-in as a constant */
    #define ${PREFIX}_CAP ((int)($CAP))
}

@struct {
    typedef struct {
        $T   slots[${PREFIX}_CAP];
        int  used;
    } ${PREFIX}_t;
}

@fn init {
    static inline void
    ${PREFIX}_init(${PREFIX}_t *p) {
        memset(p->slots, 0, sizeof(p->slots));
        p->used = 0;
    }
}

@fn alloc {
    static inline $T *
    ${PREFIX}_alloc(${PREFIX}_t *p) {
        assert(p->used < ${PREFIX}_CAP);
        return &p->slots[p->used++];
    }
}

@fn reset {
    static inline void
    ${PREFIX}_reset(${PREFIX}_t *p) {
        p->used = 0;
    }
}

@fn full {
    static inline int
    ${PREFIX}_full(const ${PREFIX}_t *p) {
        return p->used >= ${PREFIX}_CAP;
    }
}
```
---

## Standard library (`.q` modules)

`domlibs/domqlib/*.q` ships a standard library of generic, header-only C
libraries. Every module follows the `stack.q` pattern: a required element
parameter (usually `T`), a `PREFIX` defaulting to the module name, `@guard`,
and `static inline` functions. Function-valued parameters (`CMP`, `HASH`,
`EQ`) are caller-supplied C functions, declared before the header include.

| Module | Key params | Contents |
|---|---|---|
| `stack` | `T` | LIFO stack: init/push/pop/peek/empty/free |
| `mempool` | `T`, `CAP` | fixed arena: init/alloc/reset/full |
| `vector` | `T` | malloc-backed dynamic array with reserve |
| `array` | `T`, `N` | fixed-size array with checked get/set/fill |
| `deque` | `T`, `CAP` | circular-buffer double-ended queue |
| `hash` | `KEY`, `T`, `HASH`, `EQ`, `CAP` | open-addressing map, cluster-rehash remove |
| `map` | `KEY`, `T`, `CMP` | sorted dynamic-array map (binary search) |
| `set` | `T`, `CMP` | sorted dynamic-array set |
| `string` | — | char string builder: append/reserve/cstr |
| `bit` | `N` | static bitset: set/clr/test/count |
| `avl` | `KEY`, `T`, `CMP` | AVL tree map |
| `btree` | `KEY`, `T`, `CMP`, `DEG` | CLRS B-tree map |
| `graph` | `NODE_CAP`, `EDGE_CAP` | static directed graph, adjacency lists |
| `arena` | `CAP` | bump-pointer arena with alignment |
| `alloc` | — | counting malloc/calloc/free + stats |
| `csp` | `T`, `CAP` | bounded channel: send/recv/avail/space |
| `exec` | — | popen capture + pclose status decoding |
| `expr` | `SCALAR` | recursive-descent arithmetic evaluator |
| `glob` | — | `*`/`?`/`[...]` whole-string matcher |
| `regexp` | — | Pike-style regex: match leftmost, anchored full |
| `netfile` | — | length-prefixed messages over an fd |
| `netif` | — | hostname + IPv4 parse/format |
| `thread` | — | pthread mutex + spawn/join wrapper |
| `xopen` | — | whole-file read/write/exists |
| `xmm128` | — | 4×f32 SIMD (SSE2 baseline, no extra flags) |
| `xmm256` | — | 8×f32 SIMD (compile with `-mavx`) |
| `xmm512` | — | 16×f32 SIMD (compile with `-mavx512f`) |

Covered by `tests/domqlib-test-suite/` (117 cases: compile, gcc-build,
and run checks per module).

---

## Design decisions worth noting

**Why `tkinter.Tcl` and not Python f-strings or Jinja?**
Tcl's `subst` is the right primitive here. It handles `$VAR` and `${VAR}` natively, stops at whitespace without configuration, and doesn't treat C code as a template language. F-strings require Python to own the string at compile time. Jinja brings a full tag language that would be overkill and would clash with C's own `{` `}` syntax.

**Why `subst -nocommands` by default?**
C templates almost never need computed substitution. Disabling `[...]` evaluation by default means a stray `[0]` array subscript in a template is passed through literally rather than crashing the Tcl evaluator. Opt-in `-tcl` blocks are there for the rare case where you need it.

**Why hashed include guards instead of a user-supplied symbol?**
Users shouldn't have to invent unique guard symbols for every instantiation. The hash is deterministic (same params → same guard), short enough to read, and avoids the `Q_STACK_H` / `Q_STACK_H` collision when you instantiate the same module twice for different types.

**Why a flat node list instead of a tree AST?**
Q is deliberately shallow — a module is a linear sequence of declarations followed by blocks. A flat list is trivially correct and requires no tree-walking infrastructure. If Q ever needed conditional blocks (`@if`) it would graduate to a proper AST at that point.

**Why pass templates through a Tcl variable instead of `subst {...}`?**
Interpolating the template into a braced script string terminates at the first `}` in the C code, corrupting every non-trivial block. Routing it through `__q_template` keeps brace handling entirely inside `subst`.

**Why must the compiler enter `compilation_guard`?**
`resolve_imports` detects cycles by consulting the in-progress set, but nothing populates that set unless compilation itself registers. Without the guard, a cyclic `@import` recurses until the stack overflows instead of raising `CycleError`.

**Why `setuptools.build_meta`?**
`setuptools.backends.legacy:build` no longer exists in modern setuptools (84+). `build_meta` is the standard backend and builds this package identically.

---

## `pyproject.toml`

```toml
[build-system]
requires = ["setuptools>=68"]
build-backend = "setuptools.build_meta"

[project]
name = "q-lang"
version = "0.1.0"
description = "Template compiler for header-only C libraries"
requires-python = ">=3.11"
dependencies = []          # tkinter is stdlib; no third-party deps

[project.scripts]
qc = "q.cli:main"

[tool.setuptools.packages.find]
where = ["."]
include = ["q*"]
```
No third-party dependencies — `tkinter` ships with CPython on every platform Q targets.

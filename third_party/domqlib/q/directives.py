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

"""File resolution, @import graph, cycle detection."""

import contextlib
from pathlib import Path
from typing import Iterator


class CycleError(Exception):
    """Raised when an @import cycle is detected."""


_in_progress: set[str] = set()


def resolve_imports(module_name: str, search_path: list[str]) -> str:
    """Locate module_name.q on the search path and return its absolute path.

    Raises FileNotFoundError if not found, CycleError if the module is
    already being compiled (import cycle).
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

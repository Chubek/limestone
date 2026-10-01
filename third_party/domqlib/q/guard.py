"""Include-guard symbol generation."""

import hashlib
import re


def make_guard_symbol(module: str, params: dict) -> str:
    """Derive a collision-resistant include-guard symbol from the module
    name and its bound parameter values.

    e.g. make_guard_symbol("stack", {"T": "int", "PREFIX": "stack"})
         -> "Q_STACK_9e1a2b_H" (hex fragment varies with the bindings)
    """
    # Sort for determinism: same params -> same guard.
    fingerprint = ",".join(f"{k}={v}" for k, v in sorted(params.items()))
    digest = hashlib.sha1(fingerprint.encode()).hexdigest()[:6]
    safe_name = re.sub(r"[^A-Z0-9_]", "_", module.upper())
    return f"Q_{safe_name}_{digest}_H"

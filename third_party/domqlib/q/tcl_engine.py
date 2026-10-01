"""Thin wrapper around tkinter.Tcl for Q template substitution."""

import tkinter


class TclEngine:
    """Wraps a tkinter.Tcl interpreter for template substitution."""

    def __init__(self) -> None:
        self._tcl = tkinter.Tcl()

    def bind(self, name: str, value: str) -> None:
        """Set a Tcl variable."""
        self._tcl.setvar(name, value)

    def get(self, name: str) -> str:
        """Read back a Tcl variable."""
        return self._tcl.getvar(name)

    def subst(self, template: str, allow_commands: bool = False) -> str:
        """Substitute $variables and ${variables} in template.

        By default no command substitution ([...]) is performed.

        The template is passed to Tcl through a variable rather than
        interpolated into a braced script string, so that braces (or
        brackets) inside C code cannot break out of the subst call.
        """
        self._tcl.setvar("__q_template", template)
        if allow_commands:
            return self._tcl.eval("subst $__q_template")
        return self._tcl.eval("subst -nocommands $__q_template")

    def eval(self, expr: str) -> str:
        """Evaluate a Tcl expression (used for @require conditions).

        The condition is passed through a variable so that braces inside
        it cannot break out of the expr call.
        """
        self._tcl.setvar("__q_cond", expr)
        return self._tcl.eval("expr $__q_cond")

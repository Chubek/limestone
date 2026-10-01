"""Compile DomPyBind's Q modules for native consumers."""
import sys
from pathlib import Path

root = Path(__file__).resolve().parent
sys.path.insert(0, str(root.parent / "domqlib"))
from q import QCompiler

compiler = QCompiler(search_path=[str(root), str(root.parent / "domqlib")])
output = Path(sys.argv[1])
output.parent.mkdir(parents=True, exist_ok=True)
# The C++ layer needs object, interpreter and call primitives. The full C
# aggregate remains available independently (some container templates use C).
output.write_text("\n".join(compiler.compile(str(root / f"{name}.q"))
                              for name in ("pyinit", "pyobj", "pycall")), encoding="utf-8")

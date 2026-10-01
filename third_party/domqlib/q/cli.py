"""qc -- Q language compiler CLI.

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
        description="Compile a .q template to a C header.",
    )
    p.add_argument("file", help="Path to the .q source file")
    p.add_argument(
        "params",
        nargs="*",
        metavar="PARAM=VALUE",
        help="Parameter bindings, e.g. T=int PREFIX=mystack",
    )
    p.add_argument(
        "-o",
        "--output",
        metavar="FILE",
        help="Write output to FILE (default: <module>_<T>.h)",
    )
    p.add_argument(
        "-I",
        "--include",
        action="append",
        default=[],
        metavar="DIR",
        help="Add DIR to the module search path",
    )
    p.add_argument(
        "--stdout", action="store_true", help="Print generated header to stdout"
    )

    args = p.parse_args(argv)

    # Parse PARAM=VALUE pairs.
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

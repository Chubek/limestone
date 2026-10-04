#!/usr/bin/env python3
"""
C Source Code Documentation Generator using pycparser

Generates a Structured Dump File (SDF) documenting C source files with:
- File and directory information
- Dependency graph between files
- Literal contents of the files
- Summary of the files
- S-Expression of header files
- S-Expression of implementation files
"""

import argparse
import os
import sys
from pathlib import Path
from pycparser import parse_file, c_ast, c_generator
from pycparser.plyparser import ParseError


class CFileAnalyzer(c_ast.NodeVisitor):
    """Visitor to extract declarations and definitions from C AST"""

    def __init__(self):
        self.functions = []
        self.structs = []
        self.typedefs = []
        self.enums = []
        self.variables = []

    def visit_FuncDef(self, node):
        func_name = node.decl.name
        coord = node.decl.coord
        gen = c_generator.CGenerator()
        func_decl = gen.visit(node.decl)
        self.functions.append({
            'name': func_name,
            'signature': func_decl,
            'coord': str(coord) if coord else 'unknown',
            'type': 'definition'
        })
        self.generic_visit(node)

    def visit_Decl(self, node):
        if isinstance(node.type, c_ast.FuncDecl):
            gen = c_generator.CGenerator()
            func_decl = gen.visit(node)
            self.functions.append({
                'name': node.name,
                'signature': func_decl,
                'coord': str(node.coord) if node.coord else 'unknown',
                'type': 'declaration'
            })
        elif isinstance(node.type, c_ast.Struct):
            if node.type.name:
                gen = c_generator.CGenerator()
                struct_def = gen.visit(node.type)
                self.structs.append({
                    'name': node.type.name,
                    'definition': struct_def,
                    'coord': str(node.coord) if node.coord else 'unknown'
                })
        elif node.name:
            gen = c_generator.CGenerator()
            var_decl = gen.visit(node)
            self.variables.append({
                'name': node.name,
                'declaration': var_decl,
                'coord': str(node.coord) if node.coord else 'unknown'
            })
        self.generic_visit(node)

    def visit_Typedef(self, node):
        gen = c_generator.CGenerator()
        typedef_str = gen.visit(node)
        self.typedefs.append({
            'name': node.name,
            'definition': typedef_str,
            'coord': str(node.coord) if node.coord else 'unknown'
        })
        self.generic_visit(node)

    def visit_Struct(self, node):
        if node.name and node.decls:
            gen = c_generator.CGenerator()
            struct_def = gen.visit(node)
            if not any(s['name'] == node.name for s in self.structs):
                self.structs.append({
                    'name': node.name,
                    'definition': struct_def,
                    'coord': str(node.coord) if node.coord else 'unknown'
                })
        self.generic_visit(node)

    def visit_Enum(self, node):
        gen = c_generator.CGenerator()
        enum_def = gen.visit(node)
        enum_name = node.name if node.name else 'anonymous'
        self.enums.append({
            'name': enum_name,
            'definition': enum_def,
            'coord': str(node.coord) if node.coord else 'unknown'
        })
        self.generic_visit(node)


def extract_includes(filepath):
    """Extract #include directives from a file"""
    includes = []
    try:
        with open(filepath, 'r', encoding='utf-8', errors='ignore') as f:
            for line in f:
                line = line.strip()
                if line.startswith('#include'):
                    includes.append(line)
    except Exception as e:
        print(f"Warning: Could not read {filepath} for includes: {e}", file=sys.stderr)
    return includes


def ast_to_sexp(node, indent=0):
    """Convert AST node to S-expression format"""
    if node is None:
        return "nil"

    node_type = node.__class__.__name__
    result = "(" + node_type

    attrs = []
    for attr_name in node.attr_names:
        attr_value = getattr(node, attr_name, None)
        if attr_value is not None:
            attrs.append(f":{attr_name} {repr(attr_value)}")

    if attrs:
        result += " " + " ".join(attrs)

    for child_name, child in node.children():
        result += "\n" + "  " * (indent + 1)
        if isinstance(child, list):
            result += f"({child_name}"
            for item in child:
                result += "\n" + "  " * (indent + 2) + ast_to_sexp(item, indent + 2)
            result += ")"
        else:
            result += f"({child_name} {ast_to_sexp(child, indent + 1)})"

    result += ")"
    return result


def find_fake_libc_headers(script_dir, env_var, cli_arg):
    """Find fake_libc_include directory with fallback hierarchy"""
    # 1. Check script directory
    local_path = script_dir / 'fake_libc_include'
    if local_path.is_dir():
        return str(local_path)
    
    # 2. Check environment variable
    if env_var and Path(env_var).is_dir():
        return env_var
    
    # 3. Check CLI argument
    if cli_arg and Path(cli_arg).is_dir():
        return cli_arg
    
    return None


def build_cpp_args(fake_libc_path, headers_path, intf_dir, extra_args, use_real_libc):
    """Build preprocessor arguments list"""
    args = []
    
    # Add fake libc headers unless using real ones
    if not use_real_libc and fake_libc_path:
        args.append(f'-I{fake_libc_path}')
    
    # Add custom header paths from environment variable
    if headers_path:
        for path in headers_path.split(':'):
            path = path.strip()
            if path:
                args.append(f'-I{path}')
    
    # Add interface directory if specified
    if intf_dir:
        args.append(f'-I{intf_dir}')
    
    # Add user-provided extra arguments
    if extra_args:
        if isinstance(extra_args, str):
            args.extend(extra_args.split())
        else:
            args.extend(extra_args)
    
    return args


def analyze_c_file(filepath, cpp_path, cpp_args, pedantic=False):
    """Analyze a C file and extract AST and declarations"""
    try:
        ast = parse_file(filepath, use_cpp=True, cpp_path=cpp_path, cpp_args=cpp_args)
        analyzer = CFileAnalyzer()
        analyzer.visit(ast)
        includes = extract_includes(filepath)
        return {
            'ast': ast,
            'functions': analyzer.functions,
            'structs': analyzer.structs,
            'typedefs': analyzer.typedefs,
            'enums': analyzer.enums,
            'variables': analyzer.variables,
            'includes': includes,
            'error': None
        }
    except ParseError as e:
        if pedantic:
            raise
        print(f"Parse error in {filepath}: {e}", file=sys.stderr)
        return {
            'ast': None,
            'functions': [],
            'structs': [],
            'typedefs': [],
            'enums': [],
            'variables': [],
            'includes': extract_includes(filepath),
            'error': str(e)
        }
    except Exception as e:
        if pedantic:
            raise
        print(f"Error analyzing {filepath}: {e}", file=sys.stderr)
        return {
            'ast': None,
            'functions': [],
            'structs': [],
            'typedefs': [],
            'enums': [],
            'variables': [],
            'includes': extract_includes(filepath),
            'error': str(e)
        }


def match_interface_implementation(intf_files, impl_files):
    """Match interface files with implementation files by basename"""
    pairs = []
    unmatched_intf = []
    unmatched_impl = list(impl_files)

    for intf_path in intf_files:
        intf_name = Path(intf_path).stem
        matched = False
        for impl_path in impl_files:
            if intf_name == Path(impl_path).stem:
                pairs.append((intf_path, impl_path))
                if impl_path in unmatched_impl:
                    unmatched_impl.remove(impl_path)
                matched = True
                break
        if not matched:
            unmatched_intf.append(intf_path)

    return pairs, unmatched_intf, unmatched_impl


def write_sdf_header(out, files_info):
    """Write file and directory information section"""
    out.write("=" * 80 + "\n")
    out.write("STRUCTURED DUMP FILE (SDF)\n")
    out.write("=" * 80 + "\n\n")
    
    out.write("[FILE AND DIRECTORY INFORMATION]\n\n")
    for info in files_info:
        out.write(f"File: {info['path']}\n")
        out.write(f"  Type: {info['type']}\n")
        out.write(f"  Size: {info['size']} bytes\n")
        if info.get('pair'):
            out.write(f"  Paired with: {info['pair']}\n")
        out.write("\n")


def write_dependency_graph(out, all_analyses):
    """Write dependency graph section"""
    out.write("\n" + "=" * 80 + "\n")
    out.write("[DEPENDENCY GRAPH]\n\n")
    
    for filepath, analysis in all_analyses.items():
        if analysis['includes']:
            out.write(f"{filepath}:\n")
            for inc in analysis['includes']:
                out.write(f"  -> {inc}\n")
            out.write("\n")


def write_file_contents(out, files):
    """Write literal contents of files section"""
    out.write("\n" + "=" * 80 + "\n")
    out.write("[LITERAL FILE CONTENTS]\n\n")
    
    for filepath in files:
        out.write(f"--- {filepath} ---\n")
        try:
            with open(filepath, 'r', encoding='utf-8', errors='ignore') as f:
                out.write(f.read())
        except Exception as e:
            out.write(f"// Error reading file: {e}\n")
        out.write("\n\n")


def write_summary(out, all_analyses):
    """Write summary section"""
    out.write("\n" + "=" * 80 + "\n")
    out.write("[SUMMARY]\n\n")
    
    for filepath, analysis in all_analyses.items():
        out.write(f"File: {filepath}\n")
        
        if analysis['error']:
            out.write(f"  ERROR: {analysis['error']}\n\n")
            continue
        
        out.write(f"  Functions: {len(analysis['functions'])}\n")
        if analysis['functions']:
            for func in analysis['functions']:
                out.write(f"    - {func['name']} [{func['type']}]\n")
        
        out.write(f"  Structures: {len(analysis['structs'])}\n")
        if analysis['structs']:
            for struct in analysis['structs']:
                out.write(f"    - {struct['name']}\n")
        
        out.write(f"  Typedefs: {len(analysis['typedefs'])}\n")
        if analysis['typedefs']:
            for typedef in analysis['typedefs']:
                out.write(f"    - {typedef['name']}\n")
        
        out.write(f"  Enumerations: {len(analysis['enums'])}\n")
        if analysis['enums']:
            for enum in analysis['enums']:
                out.write(f"    - {enum['name']}\n")
        
        out.write(f"  Global Variables: {len(analysis['variables'])}\n")
        if analysis['variables']:
            for var in analysis['variables']:
                out.write(f"    - {var['name']}\n")
        
        out.write("\n")


def write_sexp_section(out, title, files_analyses):
    """Write S-expression section for a group of files"""
    out.write("\n" + "=" * 80 + "\n")
    out.write(f"[{title}]\n\n")
    
    for filepath, analysis in files_analyses.items():
        if analysis['ast']:
            out.write(f"Source: {filepath}\n")
            out.write("=" * 5 + "\n")
            out.write(ast_to_sexp(analysis['ast']))
            out.write("\n" + "=" * 5 + "\n\n")
        else:
            out.write(f"Source: {filepath}\n")
            out.write("=" * 5 + "\n")
            out.write(f"ERROR: Could not parse file\n")
            if analysis['error']:
                out.write(f"Details: {analysis['error']}\n")
            out.write("=" * 5 + "\n\n")


def main():
    parser = argparse.ArgumentParser(
        description='Generate Structured Dump File (SDF) from C source files using pycparser'
    )
    parser.add_argument('files', nargs='*', help='C source files to document')
    parser.add_argument('-o', '--output', default='dump.sdf',
                        help='Output file (default: dump.sdf)')
    parser.add_argument('--intf-dir', help='Interface directory (e.g., include/)')
    parser.add_argument('--impl-dir', help='Implementation directory (e.g., src/)')
    parser.add_argument('--cpp-args', help='Additional arguments for C preprocessor')
    parser.add_argument('--cpp-cmd', help='Preprocessor command (default: cpp)')
    parser.add_argument('--fake-libc-headers', help='Path to fake_libc_include directory')
    parser.add_argument('--use-real-libc-headers', action='store_true',
                        help='Use real libc headers instead of fake ones')
    parser.add_argument('--pedantic', action='store_true',
                        help='Stop on errors instead of ignoring them')

    args = parser.parse_args()

    # Determine script directory
    script_dir = Path(__file__).parent.resolve()

    # Find fake_libc_include directory
    fake_libc_env = os.environ.get('DUMPC_FAKE_LIBC_HEADERS')
    fake_libc_path = find_fake_libc_headers(script_dir, fake_libc_env, args.fake_libc_headers)
    
    if not args.use_real_libc_headers and not fake_libc_path:
        print("Warning: fake_libc_include not found. Parsing may fail on system headers.", file=sys.stderr)
        print("  Searched in:", file=sys.stderr)
        print(f"    1. {script_dir / 'fake_libc_include'}", file=sys.stderr)
        if fake_libc_env:
            print(f"    2. $DUMPC_FAKE_LIBC_HEADERS: {fake_libc_env}", file=sys.stderr)
        if args.fake_libc_headers:
            print(f"    3. --fake-libc-headers: {args.fake_libc_headers}", file=sys.stderr)
        print("  Use --use-real-libc-headers to suppress this warning.", file=sys.stderr)

    # Get custom header paths
    headers_path = os.environ.get('DUMPC_HEADERS_PATH')

    # Determine preprocessor command
    cpp_cmd = args.cpp_cmd or os.environ.get('DUMPC_CPP_CMD', 'cpp')

    # Build preprocessor arguments
    cpp_args = build_cpp_args(
        fake_libc_path,
        headers_path,
        args.intf_dir,
        args.cpp_args,
        args.use_real_libc_headers
    )

    all_files = []
    files_info = []
    all_analyses = {}
    header_analyses = {}
    impl_analyses = {}

    if args.intf_dir and args.impl_dir:
        # Interface/Implementation pairing mode
        intf_files = list(Path(args.intf_dir).rglob('*.h'))
        impl_files = list(Path(args.impl_dir).rglob('*.c'))

        pairs, unmatched_intf, unmatched_impl = match_interface_implementation(
            [str(f) for f in intf_files],
            [str(f) for f in impl_files]
        )

        # Process pairs
        for intf, impl in pairs:
            all_files.extend([intf, impl])
            files_info.append({
                'path': intf,
                'type': 'interface',
                'size': Path(intf).stat().st_size,
                'pair': impl
            })
            files_info.append({
                'path': impl,
                'type': 'implementation',
                'size': Path(impl).stat().st_size,
                'pair': intf
            })

            print(f"Processing pair: {Path(intf).name} <-> {Path(impl).name}", file=sys.stderr)
            
            intf_analysis = analyze_c_file(intf, cpp_cmd, cpp_args, args.pedantic)
            impl_analysis = analyze_c_file(impl, cpp_cmd, cpp_args, args.pedantic)
            
            all_analyses[intf] = intf_analysis
            all_analyses[impl] = impl_analysis
            header_analyses[intf] = intf_analysis
            impl_analyses[impl] = impl_analysis

        # Process unmatched files (ignored but marked)
        for intf in unmatched_intf:
            print(f"Warning: Unmatched interface file (ignored): {intf}", file=sys.stderr)
        
        for impl in unmatched_impl:
            print(f"Warning: Unmatched implementation file (ignored): {impl}", file=sys.stderr)

    elif args.files:
        # Individual files mode
        for pattern in args.files:
            path = Path(pattern)
            if path.is_file():
                all_files.append(str(path))
            elif path.is_dir():
                all_files.extend([str(f) for f in path.rglob('*.c')])
                all_files.extend([str(f) for f in path.rglob('*.h')])

        for filepath in all_files:
            file_type = 'header' if filepath.endswith('.h') else 'implementation'
            files_info.append({
                'path': filepath,
                'type': file_type,
                'size': Path(filepath).stat().st_size
            })

            print(f"Processing {filepath}...", file=sys.stderr)
            analysis = analyze_c_file(filepath, cpp_cmd, cpp_args, args.pedantic)
            all_analyses[filepath] = analysis
            
            if file_type == 'header':
                header_analyses[filepath] = analysis
            else:
                impl_analyses[filepath] = analysis
    else:
        print("No files specified. Use files argument or --intf-dir/--impl-dir", file=sys.stderr)
        return 1

    # Write SDF file
    with open(args.output, 'w', encoding='utf-8') as out:
        write_sdf_header(out, files_info)
        write_dependency_graph(out, all_analyses)
        write_file_contents(out, all_files)
        write_summary(out, all_analyses)
        
        if header_analyses:
            write_sexp_section(out, "S-EXPRESSION OF HEADER FILES", header_analyses)
        
        if impl_analyses:
            write_sexp_section(out, "S-EXPRESSION OF IMPLEMENTATION FILES", impl_analyses)

    print(f"Structured dump written to {args.output}")
    return 0


if __name__ == '__main__':
    sys.exit(main())

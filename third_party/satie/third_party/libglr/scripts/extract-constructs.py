#!/usr/bin/env python3
"""
extract-constructs.py - Extract C constructs using PyCParser

This script parses C source files using PyCParser and extracts all
constructs (data structures, functions, typedefs, macros, etc.) and
saves them to a structured output format (JSON, YAML, or S-Expression).

Usage:
    python3 extract-constructs.py <source_file> [output_format] [output_file]

Arguments:
    source_file     Path to C source file to analyze
    output_format   Output format: 'json', 'yaml', or 'sexp' (default: json)
    output_file     Output file path (default: stdout)

Requirements:
    - pycparser (pip install pycparser)

Example:
    python3 extract-constructs.py include/glr/grammar.h json > grammar.json
    python3 extract-constructs.py src/glr/grammar.c yaml > grammar.yaml
"""

import sys
import os
import argparse
import json
import ast

# Try to import pycparser
try:
    from pycparser import c_parser, c_generator, c_ast
except ImportError:
    print("Error: pycparser not found. Install with: pip install pycparser", file=sys.stderr)
    sys.exit(1)


class ConstructExtractor(c_ast.NodeVisitor):
    """Extract C constructs from AST."""
    
    def __init__(self):
        self.constructs = []
        self.generator = c_ast.CGenerator()
    
    def visit_FunctionDef(self, node):
        """Extract function definitions."""
        func_name = node.decl.name
        func_type = node.decl.type
        func_decl = {
            'type': 'function',
            'name': func_name,
            'return_type': self._get_type_name(func_type.type),
            'parameters': [],
            'line': node.line if hasattr(node, 'line') else 0
        }
        
        # Extract parameters
        if func_type.params:
            for param in func_type.params.values:
                param_info = {
                    'name': param.name if hasattr(param, 'name') else None,
                    'type': self._get_type_name(param.type)
                }
                func_decl['parameters'].append(param_info)
        
        self.constructs.append(func_decl)
        self.generic_visit(node)
    
    def visit_Decl(self, node):
        """Extract declarations (variables, typedefs, structs)."""
        if node.name:
            decl = {
                'name': node.name,
                'line': node.line if hasattr(node, 'line') else 0
            }
            
            # Check if it's a typedef
            if node.typedefs:
                decl['type'] = 'typedef'
                decl['underlying_type'] = self._get_type_name(node.type)
                self.constructs.append(decl)
            
            # Check if it's a struct/union
            elif isinstance(node.type, c_ast.Struct):
                decl['type'] = 'struct'
                decl['name'] = node.type.name if node.type.name else 'anonymous'
                decl['fields'] = self._extract_struct_fields(node.type)
                self.constructs.append(decl)
            
            elif isinstance(node.type, c_ast.Union):
                decl['type'] = 'union'
                decl['name'] = node.type.name if node.type.name else 'anonymous'
                decl['fields'] = self._extract_struct_fields(node.type)
                self.constructs.append(decl)
            
            # Regular variable/typedef
            else:
                decl['type'] = 'variable' if not node.storage else 'storage'
                decl['type_name'] = self._get_type_name(node.type)
                self.constructs.append(decl)
        
        self.generic_visit(node)
    
    def visit_Typename(self, node):
        """Extract typename (used in typedefs)."""
        if node.name and node.name not in [c['name'] for c in self.constructs]:
            self.constructs.append({
                'type': 'typedef',
                'name': node.name,
                'line': node.line if hasattr(node, 'line') else 0
            })
        self.generic_visit(node)
    
    def visit_Epoch(self, node):
        """Handle struct/union field extraction."""
        self.generic_visit(node)
    
    def _extract_struct_fields(self, struct_node):
        """Extract fields from struct/union."""
        fields = []
        if struct_node.decls:
            for decl in struct_node.decls:
                field = {
                    'name': decl.name if hasattr(decl, 'name') else None,
                    'type': self._get_type_name(decl.type) if hasattr(decl, 'type') else None
                }
                fields.append(field)
        return fields
    
    def _get_type_name(self, type_node):
        """Get type name from AST node."""
        if type_node is None:
            return 'void'
        
        if isinstance(type_node, c_ast.TypeDecl):
            return self._get_type_name(type_node.type) if type_node.type else 'unknown'
        
        elif isinstance(type_node, c_ast.IdentifierType):
            return ' '.join(type_node.names) if type_node.names else 'unknown'
        
        elif isinstance(type_node, c_ast.PtrDecl):
            base = self._get_type_name(type_node.type) if type_node.type else 'void'
            return f'{base}*'
        
        elif isinstance(type_node, c_ast.ArrayDecl):
            base = self._get_type_name(type_node.type) if type_node.type else 'unknown'
            return f'{base}[]'
        
        elif isinstance(type_node, c_ast.Struct):
            return f'struct {type_node.name if type_node.name else "anonymous"}'
        
        elif isinstance(type_node, c_ast.Union):
            return f'union {type_node.name if type_node.name else "anonymous"}'
        
        elif isinstance(type_node, c_ast.FuncDecl):
            return 'function'
        
        else:
            return type(type_node).__name__
    
    def get_constructs(self):
        """Return all extracted constructs."""
        return self.constructs


def extract_from_file(filepath):
    """Extract constructs from a C file."""
    if not os.path.exists(filepath):
        print(f"Error: File not found: {filepath}", file=sys.stderr)
        sys.exit(1)
    
    # Read source
    with open(filepath, 'r') as f:
        source = f.read()
    
    # Parse
    parser = c_parser.CParser()
    try:
        ast_root = parser.parse(source, filename=filepath)
    except Exception as e:
        print(f"Error parsing file: {e}", file=sys.stderr)
        sys.exit(1)
    
    # Extract
    extractor = ConstructExtractor()
    ast_root.accept(extractor)
    
    return extractor.get_constructs()


def output_json(constructs, output_file):
    """Output constructs as JSON."""
    json.dump(constructs, output_file, indent=2)


def output_yaml(constructs, output_file):
    """Output constructs as YAML (using simple formatting)."""
    for i, construct in enumerate(constructs):
        if i > 0:
            output_file.write('\n---\n')
        output_file.write(f"# {construct.get('type', 'unknown')} {construct.get('name', 'unnamed')}\n")
        for key, value in construct.items():
            if key not in ('type', 'name'):
                output_file.write(f'  {key}: {value}\n')


def output_sexp(constructs, output_file):
    """Output constructs as S-Expression."""
    def to_sexp(obj):
        if isinstance(obj, dict):
            parts = [f"({obj.get('type', 'unknown')}", 
                    f"(name \"{obj.get('name', 'unnamed')}\")"]
            for key, value in obj.items():
                if key not in ('type', 'name'):
                    if isinstance(value, list):
                        parts.append(f"({key} {' '.join(str(v) for v in value)})")
                    else:
                        parts.append(f"({key} {value})")
            parts.append(")")
            return ' '.join(parts)
        elif isinstance(obj, list):
            return ' '.join(to_sexp(item) for item in obj)
        else:
            return str(obj)
    
    output_file.write(f"(constructs {' '.join(to_sexp(c) for c in constructs)})\n")


def main():
    parser = argparse.ArgumentParser(
        description='Extract C constructs from source files using PyCParser'
    )
    parser.add_argument('source_file', help='C source file to analyze')
    parser.add_argument(
        'output_format',
        nargs='?',
        choices=['json', 'yaml', 'sexp'],
        default='json',
        help='Output format (default: json)'
    )
    parser.add_argument(
        'output_file',
        nargs='?',
        help='Output file path (default: stdout)'
    )
    
    args = parser.parse_args()
    
    # Extract constructs
    constructs = extract_from_file(args.source_file)
    
    # Output
    if args.output_file:
        with open(args.output_file, 'w') as f:
            if args.output_format == 'json':
                output_json(constructs, f)
            elif args.output_format == 'yaml':
                output_yaml(constructs, f)
            elif args.output_format == 'sexp':
                output_sexp(constructs, f)
    else:
        if args.output_format == 'json':
            output_json(constructs, sys.stdout)
        elif args.output_format == 'yaml':
            output_yaml(constructs, sys.stdout)
        elif args.output_format == 'sexp':
            output_sexp(constructs, sys.stdout)
    
    print(f"\nExtracted {len(constructs)} constructs from {args.source_file}",
          file=sys.stderr)


if __name__ == '__main__':
    main()

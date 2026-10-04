#!/usr/bin/env bash
set -euo pipefail
# libglr SWIG Binding Generator with Cache API and LMDB support

SCRIPT_DIR=$(CDPATH= cd -- "$(dirname -- "$0")" && pwd)
REPO_ROOT=$(CDPATH= cd -- "$SCRIPT_DIR/.." && pwd)
INTERFACE_FILE="$SCRIPT_DIR/libglr.i"
DEFAULT_OUTPUT_ROOT="$REPO_ROOT"

show_help() {
  cat <<'USAGE'
Usage: bindings/generate-bindings.sh [options]

Generate SWIG bindings (with optional Cache API/LMDB support) into directories named `_bindings_libglr_<language>`.
Directories are created in the repository root by default.

Options:
  --language <name>   Generate one binding target; may be repeated.
  --all               Generate bindings for every language reported by SWIG.
  --output-root <dir> Write generated directories/archives under this root.
  --archive           Create archives for each generated binding.
  --directory         Keep the generated directory when --archive is also used.
  --package           Create a packaging layout for each generated binding.
  --git               Initialize a Git repository in each generated directory.
  --doc               Add binding-specific notes to each generated directory.
  --examples          Add starter examples to each generated directory.
  --gzip              Use gzip archives (default archive format).
  --deb               Use deb archives.
  --rpm               Use rpm archives.
  --zlib              Use zlib archives.
  --bzip2             Use bzip2 archives.
  --pacman            Use pacman archives.
  --with-cache        Include Cache API bindings (requires LMDB).
  -h, --help          Show this help.

Notes:
  - Without --archive, generated directories are kept automatically.
  - --directory only changes behavior when --archive is also present.
  - When --all is used, missing per-language prerequisites are skipped.
USAGE
}

log() {
  printf '[bindings] %s\n' "$*"
}

warn() {
  printf '[bindings] warning: %s\n' "$*" >&2
}

fail() {
  printf '[bindings] error: %s\n' "$*" >&2
  exit 1
}

command_exists() {
  command -v "$1" >/dev/null 2>&1
}

script_usable() {
  local script_path=$1
  [[ -f "$script_path" && -s "$script_path" ]]
}

normalize_language() {
  printf '%s' "$1" | tr '[:upper:]' '[:lower:]'
}

swig_supported_languages() {
  swig -help 2>/dev/null | awk '
    BEGIN { section = 0 }
    /Supported Target Language Options/ { section = 1; next }
    section && /^[-[:space:]]*$/ { exit }
    section {
      for (i = 1; i <= NF; ++i) {
        if ($i ~ /^-[A-Za-z0-9_+.-]+$/) {
          lang = substr($i, 2)
          gsub(/[,;:].*$/, "", lang)
          print tolower(lang)
        }
      }
    }
  ' | sort -u
}

require_archive_service() {
  local archive_script="$REPO_ROOT/scripts/archive-services.py"
  if ! command_exists python3; then
    fail "python3 is required to drive scripts/archive-services.py"
  fi
  if ! script_usable "$archive_script"; then
    fail "scripts/archive-services.py is missing or empty"
  fi
}

require_packaging_service() {
  local packaging_script="$REPO_ROOT/scripts/packaging-service.py"
  if ! command_exists python3; then
    fail "python3 is required to drive scripts/packaging-service.py"
  fi
  if ! script_usable "$packaging_script"; then
    fail "scripts/packaging-service.py is missing or empty"
  fi
}

language_swig_options() {
  local language=$1
  case "$language" in
    csharp)
      printf '%s\n' '-namespace' 'LibGLR'
      ;;
    java)
      printf '%s\n' '-package' 'org.libglr'
      ;;
    javascript)
      printf '%s\n' '-node'
      ;;
    ruby)
      printf '%s\n' '-initname' 'libglr'
      ;;
    *)
      :
      ;;
  esac
}

language_wrapper_extension() {
  local language=$1
  case "$language" in
    csharp|java)
      printf 'cxx\n'
      ;;
    *)
      printf 'c\n'
      ;;
  esac
}

write_git_scaffold() {
  local target_dir=$1

  cat > "$target_dir/.gitignore" <<'GITIGNORE'
*.o
*.obj
*.so
*.dll
*.dylib
*.a
*.lib
*.pyc
__pycache__/
*.class
*.jar
*.gem
pkg/
build/
dist/
out/
GITIGNORE

  : > "$target_dir/.gitmodules"

  if command_exists git; then
    if [[ ! -d "$target_dir/.git" ]]; then
      git -C "$target_dir" init >/dev/null 2>&1 || warn "git init failed in $target_dir"
    fi
  else
    warn "git requested, but git is not installed; wrote .gitignore/.gitmodules only"
  fi
}

copy_runtime_assets() {
  local target_dir=$1
  local runtime_dir="$target_dir/runtime"
  local shared_lib

  mkdir -p "$runtime_dir/include" "$runtime_dir/doc"
  cp -R "$REPO_ROOT/include/glr" "$runtime_dir/include/"

  if [[ -d "$REPO_ROOT/rewritelib" ]]; then
    cp -R "$REPO_ROOT/rewritelib" "$runtime_dir/"
  fi
  if [[ -d "$REPO_ROOT/disambstd" ]]; then
    cp -R "$REPO_ROOT/disambstd" "$runtime_dir/"
  fi
  if [[ -d "$REPO_ROOT/doc" ]]; then
    cp -R "$REPO_ROOT/doc" "$runtime_dir/"
  fi

  shared_lib=$(find "$REPO_ROOT/build" -maxdepth 2 -type f \( -name 'libglr.so' -o -name 'liblibglr.so' -o -name 'libglr.dylib' -o -name 'glr.dll' -o -name 'liblibglr.a' -o -name 'libglr.a' \) | head -n 1 || true)
  if [[ -n "$shared_lib" ]]; then
    cp "$shared_lib" "$runtime_dir/"
  else
    warn "no built libglr shared/static library found under build/"
\n  # Copy LMDB library if cache support is enabled
  lmdb_lib=$(find /usr/lib /usr/local/lib -maxdepth 2 -type f \( -name 'liblmdb.so*' -o -name 'liblmdb.dylib' -o -name 'liblmdb.a' \) 2>/dev/null | head -n 1 || true)
  if [[ -n "$lmdb_lib" ]]; then
    cp "$lmdb_lib" "$runtime_dir/" 2>/dev/null || warn "could not copy LMDB library"
  else
    warn "LMDB library not found; cache API may require manual LMDB installation"
  fi
  fi
}

write_binding_notes() {
  local target_dir=$1
  local language=$2

  cat > "$target_dir/README.md" <<'EOF2'
# libglr LANGUAGE binding

This directory contains SWIG-generated low-level bindings for libglr.

- Generated from `bindings/libglr.i`
- Intended as a thin FFI layer for higher-level wrappers.
- Helper accessors prefixed with `glr_binding_` smooth over C arrays and unions.
\n## Cache API Support
If libglr was built with `ENABLE_CACHE=ON`, these bindings include the incremental parsing Cache API.
This requires LMDB (Lightning Memory-Mapped Database) to be installed on the target system.
- `Makefile` builds and installs the binding with the language toolchain when supported.
EOF2
  python3 - "$target_dir/README.md" "$language" <<'PYEOF'
from pathlib import Path
import sys

path = Path(sys.argv[1])
language = sys.argv[2]
path.write_text(path.read_text().replace("LANGUAGE", language), encoding="utf-8")
PYEOF
}

write_binding_examples() {
  local target_dir=$1
  local language=$2
  local examples_dir="$target_dir/examples"

  mkdir -p "$examples_dir"
  case "$language" in
    python)
      cat > "$examples_dir/basic.py" <<'PYEOF'
import libglr

grammar = libglr.glr_grammar_create()
print(libglr.glr_name(), libglr.glr_version())
libglr.glr_grammar_destroy(grammar)
PYEOF
      ;;
    ruby)
      cat > "$examples_dir/basic.rb" <<'RBEOF'
require 'libglr'

grammar = Libglr.glr_grammar_create()
puts "#{Libglr.glr_name()} #{Libglr.glr_version()}"
Libglr.glr_grammar_destroy(grammar)
RBEOF
      ;;
    java)
      cat > "$examples_dir/Basic.java" <<'JAVEOF'
public final class Basic {
  public static void main(String[] args) {
    System.out.println(libglr.glr_name() + " " + libglr.glr_version());
  }
}
JAVEOF
      ;;
    csharp)
      cat > "$examples_dir/Basic.cs" <<'CSEOF'
using System;

public static class Basic {
  public static void Main() {
    Console.WriteLine(libglr.glr_name() + " " + libglr.glr_version());
  }
}
CSEOF
      ;;
    go)
      cat > "$examples_dir/basic.go" <<'GOEOF'
package main

func main() {
    _ = SwigcptrGlr_grammar_t(0)
}
GOEOF
      ;;
    *)
      cat > "$examples_dir/README.txt" <<EOF2
No language-specific example template is bundled yet for $language.
Use the generated API files together with the helpers documented in README.md.
EOF2
      ;;
  esac
}

write_python_package_files() {
  local target_dir=$1

  cat > "$target_dir/pyproject.toml" <<'EOF2'
[build-system]
requires = ["setuptools>=61"]
build-backend = "setuptools.build_meta"
EOF2

  cat > "$target_dir/setup.py" <<'EOF2'
from pathlib import Path
from setuptools import Extension, setup

root = Path(__file__).resolve().parent
runtime = root / "runtime"
include_dirs = [str(runtime / "include")]
library_dirs = [str(runtime)]
libraries = ["libglr", "glr"]
extra_link_args = []

if not any((runtime / f"lib{name}.a").exists() or (runtime / f"lib{name}.so").exists() or (runtime / f"{name}.dll").exists() for name in libraries):
    libraries = []
    extra_link_args = [str(path) for path in runtime.glob("lib*.a")]

setup(
    name="libglr-bindings",
    version="1.0.0",
    py_modules=["libglr"],
    ext_modules=[
        Extension(
            "_libglr",
            sources=["libglr_wrap.c"],
            include_dirs=include_dirs,
            library_dirs=library_dirs,
            libraries=libraries,
            extra_link_args=extra_link_args,
        )
    ],
)
EOF2
}

write_ruby_package_files() {
  local target_dir=$1

  mkdir -p "$target_dir/lib"
  cat > "$target_dir/lib/libglr.rb" <<'EOF2'
# frozen_string_literal: true

require_relative '../libglr'
EOF2

  cat > "$target_dir/libglr.gemspec" <<'EOF2'
Gem::Specification.new do |spec|
  spec.name = 'libglr-bindings'
  spec.version = '1.0.0'
  spec.summary = 'SWIG-generated libglr bindings'
  spec.require_paths = ['lib']
  spec.files = Dir.glob('{lib,examples,runtime}/**/*') + ['libglr_wrap.c', 'libglr.rb', 'Makefile']
end
EOF2
}

write_package_scaffold() {
  local target_dir=$1
  local language=$2

  case "$language" in
    python)
      write_python_package_files "$target_dir"
      ;;
    ruby)
      write_ruby_package_files "$target_dir"
      ;;
    go)
      cat > "$target_dir/go.mod" <<'EOF2'
module example.com/libglr-bindings

go 1.20
EOF2
      ;;
    java)
      mkdir -p "$target_dir/src/main/java"
      cat > "$target_dir/pom.xml" <<'EOF2'
<project xmlns="http://maven.apache.org/POM/4.0.0"
         xmlns:xsi="http://www.w3.org/2001/XMLSchema-instance"
         xsi:schemaLocation="http://maven.apache.org/POM/4.0.0 http://maven.apache.org/xsd/maven-4.0.0.xsd">
  <modelVersion>4.0.0</modelVersion>
  <groupId>org.libglr</groupId>
  <artifactId>libglr-bindings</artifactId>
  <version>1.0.0</version>
</project>
EOF2
      ;;
    csharp)
      cat > "$target_dir/libglr-bindings.csproj" <<'EOF2'
<Project Sdk="Microsoft.NET.Sdk">
  <PropertyGroup>
    <TargetFramework>net8.0</TargetFramework>
  </PropertyGroup>
</Project>
EOF2
      ;;
    *)
      cat > "$target_dir/PACKAGE.txt" <<EOF2
Packaging scaffolding for $language should be added by a higher-level wrapper.
The low-level binding artifacts are ready in this directory.
EOF2
      ;;
  esac
}

write_language_runtime_files() {
  local target_dir=$1
  local language=$2

  case "$language" in
    python)
      write_python_package_files "$target_dir"
      ;;
    ruby)
      cat > "$target_dir/libglr.rb" <<'EOF2'
# frozen_string_literal: true

require_relative 'lib/libglr'
EOF2
      write_ruby_package_files "$target_dir"
      ;;
    *)
      :
      ;;
  esac
}

write_binding_makefile() {
  local target_dir=$1
  local language=$2

  case "$language" in
    python)
      cat > "$target_dir/Makefile" <<'EOF2'
PYTHON ?= python3
PIP ?= $(PYTHON) -m pip

.PHONY: all build install clean

all: build

build:
	$(PYTHON) setup.py build_ext --inplace

install:
	$(PIP) install .

clean:
	rm -rf build *.egg-info _libglr*.so _libglr*.pyd _libglr*.dll
EOF2
      ;;
    ruby)
      cat > "$target_dir/Makefile" <<'EOF2'
RUBY ?= ruby
CC ?= cc
CFLAGS ?= -fPIC -O2
RUBY_CFLAGS := $(shell $(RUBY) -rrbconfig -e 'print RbConfig::CONFIG[%q{CFLAGS}]')
RUBY_CPPFLAGS := $(shell $(RUBY) -rrbconfig -e 'print RbConfig::CONFIG[%q{CPPFLAGS}]')
RUBY_HDRDIR := $(shell $(RUBY) -rrbconfig -e 'print RbConfig::CONFIG[%q{rubyhdrdir}]')
RUBY_ARCHHDRDIR := $(shell $(RUBY) -rrbconfig -e 'print RbConfig::CONFIG[%q{rubyarchhdrdir}]')
RUBY_LIBDIR := $(shell $(RUBY) -rrbconfig -e 'print RbConfig::CONFIG[%q{libdir}]')
RUBY_SO_NAME := $(shell $(RUBY) -rrbconfig -e 'print RbConfig::CONFIG[%q{RUBY_SO_NAME}]')
RUBY_SITEARCHDIR := $(shell $(RUBY) -rrbconfig -e 'print RbConfig::CONFIG[%q{sitearchdir}]')
RUBY_SITELIBDIR := $(shell $(RUBY) -rrbconfig -e 'print RbConfig::CONFIG[%q{sitelibdir}]')
RUBY_DLDFLAGS := $(shell $(RUBY) -rrbconfig -e 'print [RbConfig::CONFIG[%q{LIBRUBYARG_SHARED}], RbConfig::CONFIG[%q{DLDFLAGS}], RbConfig::CONFIG[%q{LDSHAREDXXFLAGS}]].compact.join(%q{ })')
LIBGLR_LINK := $(shell find runtime -maxdepth 1 -type f \( -name 'libglr.*' -o -name 'liblibglr.*' \) | head -n 1)

.PHONY: all build install clean

all: build

build: libglr.so

libglr.so: libglr_wrap.c lib/libglr.rb
	$(CC) $(CFLAGS) $(RUBY_CFLAGS) $(RUBY_CPPFLAGS) -Iruntime/include -I$(RUBY_HDRDIR) -I$(RUBY_ARCHHDRDIR) -shared libglr_wrap.c -L$(RUBY_LIBDIR) $(RUBY_DLDFLAGS) $(LIBGLR_LINK) -o $@

install: build
	install -d $(DESTDIR)$(RUBY_SITELIBDIR) $(DESTDIR)$(RUBY_SITEARCHDIR)
	install -m 0644 lib/libglr.rb $(DESTDIR)$(RUBY_SITELIBDIR)/libglr.rb
	install -m 0755 libglr.so $(DESTDIR)$(RUBY_SITEARCHDIR)/libglr.so

clean:
	rm -f libglr.so
EOF2
      ;;
    java)
      cat > "$target_dir/Makefile" <<'EOF2'
JAVAC ?= javac
JAR ?= jar

.PHONY: all build install clean

all: build

build:
	$(JAVAC) *.java
	$(JAR) cf libglr.jar *.class

install: build
	@echo "Copy libglr.jar and the native library to your Java runtime layout."

clean:
	rm -f *.class libglr.jar
EOF2
      ;;
    csharp)
      cat > "$target_dir/Makefile" <<'EOF2'
DOTNET ?= dotnet

.PHONY: all build install clean

all: build

build:
	$(DOTNET) build libglr-bindings.csproj

install: build
	$(DOTNET) pack libglr-bindings.csproj -o dist

clean:
	$(DOTNET) clean libglr-bindings.csproj
	rm -rf bin obj dist
EOF2
      ;;
    *)
      cat > "$target_dir/Makefile" <<EOF2
.PHONY: all build install clean

all: build

build:
	@echo "No build recipe is bundled yet for $language."

install:
	@echo "No installation recipe is bundled yet for $language."

clean:
	@true
EOF2
      ;;
  esac
}

run_packaging_service() {
  local language=$1
  local target_dir=$2
  local archive_format=$3
  local packaging_script="$REPO_ROOT/scripts/packaging-service.py"

  if [[ "$archive_format" == "deb" || "$archive_format" == "rpm" ]]; then
    return 0
  fi

  require_packaging_service
  python3 "$packaging_script" --language "$language" --input "$target_dir" || \
    warn "packaging-service.py reported an error for $language"
}

run_archive_service() {
  local language=$1
  local target_dir=$2
  local output_root=$3
  local archive_format=$4
  local archive_script="$REPO_ROOT/scripts/archive-services.py"

  require_archive_service
  python3 "$archive_script" --input "$target_dir" --language "$language" \
    --format "$archive_format" --output "$output_root" || \
    warn "archive-services.py reported an error for $language"
}

generate_language() {
  local language=$1
  local output_root=$2
  local archive_requested=$3
  local keep_directory=$4
  local package_requested=$5
  local git_requested=$6
  local doc_requested=$7
  local examples_requested=$8
  local archive_format=$9
  local swig_opts_file=${10}
  local target_dir="$output_root/_bindings_libglr_${language}"
  local wrapper_ext
  local swig_output

  wrapper_ext=$(language_wrapper_extension "$language")
  swig_output="$target_dir/libglr_wrap.${wrapper_ext}"

  rm -rf "$target_dir"
  mkdir -p "$target_dir"

  if ! mapfile -t swig_opts < "$swig_opts_file"; then
    swig_opts=()
  fi

  log "generating $language bindings in $target_dir"
  # Add cache support flag if LMDB is available
  local lmdb_flag=""
  if command_exists pkg-config && pkg-config --exists lmdb 2>/dev/null; then
    lmdb_flag="-DHAVE_LMDB"
    log "  including Cache API bindings (LMDB detected)"
  else
    warn "  LMDB not detected; Cache API bindings will be excluded"
  fi

  if ! swig -I"$REPO_ROOT/include" -I"$REPO_ROOT/third_party" $lmdb_flag -o "$swig_output" \
      -outdir "$target_dir" "-${language}" "${swig_opts[@]}" "$INTERFACE_FILE"; then
    rm -rf "$target_dir"
    return 1
  fi

  copy_runtime_assets "$target_dir"
  write_language_runtime_files "$target_dir" "$language"
  write_binding_makefile "$target_dir" "$language"

  if [[ "$doc_requested" == "1" ]]; then
    write_binding_notes "$target_dir" "$language"
  fi
  if [[ "$examples_requested" == "1" ]]; then
    write_binding_examples "$target_dir" "$language"
  fi
  if [[ "$package_requested" == "1" ]]; then
    write_package_scaffold "$target_dir" "$language"
    run_packaging_service "$language" "$target_dir" "$archive_format"
  fi
  if [[ "$git_requested" == "1" ]]; then
    write_git_scaffold "$target_dir"
  fi
  if [[ "$archive_requested" == "1" ]]; then
    run_archive_service "$language" "$target_dir" "$output_root" "$archive_format"
    if [[ "$keep_directory" != "1" ]]; then
      rm -rf "$target_dir"
    fi
  fi

  return 0
}

archive_format="gzip"
archive_requested=0
keep_directory=1
package_requested=0
git_requested=0
doc_requested=0
examples_requested=0
output_root="$DEFAULT_OUTPUT_ROOT"
with_cache=0
all_requested=0
languages=()

while [[ $# -gt 0 ]]; do
  case "$1" in
    --language)
      [[ $# -ge 2 ]] || fail "--language requires an argument"
      languages+=("$(normalize_language "$2")")
      shift 2
      ;;
    --all)
      all_requested=1
      shift
      ;;
    --output-root)
      [[ $# -ge 2 ]] || fail "--output-root requires an argument"
      output_root=$2
      shift 2
      ;;
    --archive)
      archive_requested=1
      shift
      ;;
    --directory)
      keep_directory=1
      shift
      ;;
    --package)
      package_requested=1
      shift
      ;;
    --git)
      git_requested=1
      shift
      ;;
    --doc)
      doc_requested=1
      shift
      ;;
    --examples)
      examples_requested=1
      shift
      ;;
    --gzip)
      archive_format="gzip"
      shift
      ;;
    --deb)
      archive_format="deb"
      shift
      ;;
    --rpm)
      archive_format="rpm"
      shift
      ;;
    --zlib)
      archive_format="zlib"
      shift
      ;;
    --bzip2)
      archive_format="bzip2"
      shift
      ;;
    --pacman)
      archive_format="pacman"
      shift
      ;;
    --with-cache)
      with_cache=1
      shift
      ;;
    -h|--help)
      show_help
      exit 0
      ;;
    *)
      fail "unknown option: $1"
      ;;
  esac
done

if ! command_exists swig; then
  fail "swig is required to generate bindings"
fi
\n# Check for LMDB if cache support is requested
if [[ "$with_cache" == "1" ]]; then
  if ! command_exists pkg-config || ! pkg-config --exists lmdb 2>/dev/null; then
    warn "LMDB not found; cache API bindings will be excluded"
  fi
fi

if [[ ! -f "$INTERFACE_FILE" ]]; then
  fail "SWIG interface file not found: $INTERFACE_FILE"
fi

mkdir -p "$output_root"

if [[ "$archive_requested" == "1" && "$keep_directory" != "1" ]]; then
  keep_directory=0
fi
if [[ "$archive_requested" != "1" ]]; then
  keep_directory=1
fi

if [[ "$all_requested" == "1" ]]; then
  mapfile -t auto_languages < <(swig_supported_languages)
  languages+=("${auto_languages[@]}")
fi

if [[ ${#languages[@]} -eq 0 ]]; then
  languages=(python)
fi

mapfile -t supported_languages < <(swig_supported_languages)
if [[ ${#supported_languages[@]} -eq 0 ]]; then
  fail "could not determine SWIG target languages"
fi

declare -A supported_map=()
for language in "${supported_languages[@]}"; do
  supported_map["$language"]=1
done

declare -A seen_languages=()
final_languages=()
for language in "${languages[@]}"; do
  language=$(normalize_language "$language")
  [[ -n "$language" ]] || continue
  if [[ -n ${seen_languages[$language]:-} ]]; then
    continue
  fi
  seen_languages["$language"]=1
  final_languages+=("$language")
done

status=0
for language in "${final_languages[@]}"; do
  opts_file=$(mktemp)
  trap 'rm -f "$opts_file"' EXIT

  if [[ -z ${supported_map[$language]:-} ]]; then
    if [[ "$all_requested" == "1" ]]; then
      warn "skipping unsupported SWIG target: $language"
      rm -f "$opts_file"
      trap - EXIT
      continue
    fi
    fail "SWIG does not report support for target language: $language"
  fi

  language_swig_options "$language" > "$opts_file"
  if ! generate_language "$language" "$output_root" "$archive_requested" \
      "$keep_directory" "$package_requested" "$git_requested" \
      "$doc_requested" "$examples_requested" "$archive_format" "$opts_file"; then
    if [[ "$all_requested" == "1" ]]; then
      warn "skipping $language after generation failure"
      status=1
    else
      rm -f "$opts_file"
      trap - EXIT
      fail "failed to generate bindings for $language"
    fi
  fi

  rm -f "$opts_file"
  trap - EXIT
done

exit $status

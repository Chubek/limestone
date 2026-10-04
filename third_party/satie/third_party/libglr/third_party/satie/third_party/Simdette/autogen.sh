#!/usr/bin/env bash
# ============================================================================
# Simdette Autotools Configuration Generator
# ============================================================================

set -euo pipefail

echo "Generating autotools files..."
echo "Note: Simdette is header-only, so autotools is minimal."

# Check for autoconf
if ! command -v autoconf &> /dev/null; then
    echo "Error: autoconf not found"
    exit 1
fi

# Check for automake
if ! command -v automake &> /dev/null; then
    echo "Error: automake not found"
    exit 1
fi

# Create configure.ac
cat > configure.ac << 'AC'
dnl ============================================================================
dnl Simdette Configuration
dnl ============================================================================
AC_INIT([simdette], [0.1.0], [https://github.com/your-repo/simdette])
AC_CONFIG_SRCDIR([include/simdette/simdette.hpp])
AC_CONFIG_HEADERS([config.h])
AC_CONFIG_MACRO_DIRS([m4])

AC_PREREQ([2.69])
AM_INIT_AUTOMAKE([-Wall -Werror foreign])
AM_MAINTAINER_MODE

dnl Checks for programs
AC_PROG_CXX
AC_PROG_CC
AC_PROG_RANLIB

dnl C++11/17/20 check
AC_CHECK_TOOL([CXX], [g++], [clang++])
AC_CHECK_TOOL([CC], [gcc], [clang])

dnl Check for C++20 support
AC_COMPILE_IFELSE(
    [AC_LANG_SOURCE([[
        #include <concepts>
        template<typename T> concept Test = true;
    ]])],
    [AC_DEFINE([HAVE_CXX20], [1], [Define if C++20 is supported])],
    [AC_MSG_ERROR([C++20 compiler required])]
)

dnl Check for SIMD support
AX_CHECK_COMPILE_FLAG([-msse2], [HAVE_SSE2=yes], [HAVE_SSE2=no])
AX_CHECK_COMPILE_FLAG([-mavx], [HAVE_AVX=yes], [HAVE_AVX=no])
AX_CHECK_COMPILE_FLAG([-mavx2], [HAVE_AVX2=yes], [HAVE_AVX2=no])
AX_CHECK_COMPILE_FLAG([-mavx512f], [HAVE_AVX512=yes], [HAVE_AVX512=no])

AC_SUBST([HAVE_SSE2])
AC_SUBST([HAVE_AVX])
AC_SUBST([HAVE_AVX2])
AC_SUBST([HAVE_AVX512])

dnl Options
AC_ARG_ENABLE([tests],
    [AS_HELP_STRING([--enable-tests], [Build test suite])],
    [enable_tests=$enableval],
    [enable_tests=no])

AC_ARG_ENABLE([examples],
    [AS_HELP_STRING([--enable-examples], [Build examples])],
    [enable_examples=$enableval],
    [enable_examples=no])

dnl Output files
AC_OUTPUT([
    Makefile
    docs/Makefile
])

AC_MSG_NOTICE([
Simdette configuration complete:
  - C++20 support: $(test $ac_cv_have_cxx20 = yes && echo yes || echo no)
  - SSE2: $HAVE_SSE2
  - AVX: $HAVE_AVX
  - AVX2: $HAVE_AVX2
  - AVX-512: $HAVE_AVX512
  - Tests: $enable_tests
  - Examples: $enable_examples
])

AC_CONFIG_COMMANDS([post], [
    echo "Run 'make' to build"
    echo "Run 'make install' to install headers"
])
AC
echo "Created configure.ac"

# Create Makefile.am
cat > Makefile.am << 'AM'
# ============================================================================
# Simdette Makefile.am
# ============================================================================

ACLOCAL_AMFLAGS = -I m4

# Subdirs
SUBDIRS = doc

# Headers to install
simdette_HEADERS = \
    include/simdette/simdette.hpp \
    include/simdette/setup/config.hpp \
    include/simdette/setup/detect_arch.hpp \
    include/simdette/core/batch.hpp \
    include/simdette/core/traits.hpp \
    include/simdette/core/memory.hpp \
    include/simdette/core/operators.hpp \
    include/simdette/math/basic_math.hpp \
    include/simdette/math/transcendental.hpp \
    include/simdette/arch/scalar/scalar.hpp \
    include/simdette/arch/scalar/register.hpp \
    include/simdette/arch/scalar/operators.hpp \
    include/simdette/arch/x86/x86.hpp \
    include/simdette/arch/x86/sse.hpp \
    include/simdette/arch/x86/avx.hpp \
    include/simdette/arch/x86/avx2.hpp \
    include/simdette/arch/x86/avx512.hpp \
    include/simdette/arch/arm/arm.hpp \
    include/simdette/arch/arm/neon.hpp \
    include/simdette/arch/arm/sve.hpp \
    include/simdette/arch/wasm/wasm.hpp \
    include/simdette/arch/wasm/simd128.hpp

# Man pages
man_MANS = docs/man/man7/simdette.7

# Documentation
doc_DATA = \
    README.md \
    LICENSE \
    INSTALL.md \
    USAGE.md

# Conditional test targets
if ENABLE_TESTS
check_PROGRAMS = test_comprehensive
test_comprehensive_SOURCES = tests/test_comprehensive.cpp
test_comprehensive_CXXFLAGS = -std=c++20 -I$(top_srcdir)/include
endif

# Conditional example targets
if ENABLE_EXAMPLES
examplesbin_PROGRAMS = example_basics example_math
example_basics_SOURCES = examples/example_basics.cpp
example_basics_CXXFLAGS = -std=c++20 -I$(top_srcdir)/include
example_math_SOURCES = examples/example_math.cpp
example_math_CXXFLAGS = -std=c++20 -I$(top_srcdir)/include
endif

# Install documentation
install-data-local:
	$(MKDIR_P) $(DESTDIR)$(docdir)/simdette
	cp -r docs/manual $(DESTDIR)$(docdir)/simdette/
AM
echo "Created Makefile.am"

# Run autoreconf
echo "Running autoreconf..."
autoreconf -fi

echo "Autotools files generated successfully!"
echo ""
echo "Next steps:"
echo "  ./configure --enable-tests --enable-examples"
echo "  make"
echo "  make check"
echo "  make install"

#!/usr/bin/env bash

QaMRpp_DIR=./third_party/QaMRpp
QaMRpp_LIB=$QaMRpp_DIR/QaMRpp-Library.c

LIB_DEST=$HOME/.qamrpp/eqvsglib

CC="${CC:-gcc}"
OPT="${OPT:-2}"


mkdir -p $LIB_DEST

rm -rf $LIB_DEST

$CC -O"$OPT" -I"$QaMRpp_DIR" "$QaMRpp_LIB" -shared -fPIC -o"$LIB_DEST/libqamrpp-sexpr.so" qamrpp-lib/S-Expression.c

$CC -O"$OPT" -I"$QaMRpp_DIR" "$QaMRpp_LIB" -shared -fPIC -o"$LIB_DEST/libqamrpp-leqvsg.so" qamrpp-lib/leqvsg.c

CXX="${CXX:-g++}"
EQVSG_DEST=$HOME/.local/bin
HEADER_DEST=$HOME/.local/include

mkdir $EQVSG_DEST
mkdir $HEADER_DEST

rm -f $EQVSG_DEST/eqvsg

$CXX -I"$QaMRpp_DIR" -o $EQVSG_DEST/eqvsg EQVSG.cpp


cp *.hpp $HEADER_DEST

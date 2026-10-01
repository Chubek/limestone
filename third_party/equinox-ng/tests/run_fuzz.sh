#!/usr/bin/env bash
set -euo pipefail

mkdir -p tests/fuzz/out

for i in $(seq 1 5); do
  timeout 2s afl-fuzz -i tests/fuzz/corpus_rewrite -o tests/fuzz/out -S rewrite_$i -- ./tests/fuzz_rewrite @@ || true
done
for i in $(seq 1 5); do
  timeout 2s afl-fuzz -i tests/fuzz/corpus_rule_loader -o tests/fuzz/out -S loader_$i -- ./tests/fuzz_rule_loader @@ || true
done

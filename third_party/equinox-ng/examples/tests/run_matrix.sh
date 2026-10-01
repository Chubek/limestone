#!/usr/bin/env bash
set -euo pipefail
ROOT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")/../.." && pwd)"
cd "$ROOT_DIR"

mkdir -p examples/outputs
: > examples/outputs/assessment.tsv
printf "expr_def	rules	input	output_file	status
" >> examples/outputs/assessment.tsv

for expr in examples/expr-definitions/*.eqdef; do
  for rules in examples/rewrite-rules/*.rwr; do
    rule_base="$(basename "$rules" .rwr)"
    input="examples/tests/input-${rule_base}.sexpr"
    if [[ ! -f "$input" ]]; then
      printf "%s	%s	%s	%s	%s
" "$expr" "$rules" "-" "-" "missing_input" >> examples/outputs/assessment.tsv
      continue
    fi

    out="examples/outputs/$(basename "$expr" .eqdef)__${rule_base}.out"
    if eqvsg "$expr" "$rules" "$input" > "$out" 2>"$out.err"; then
      in_norm="$(tr -d '
' < "$input" | sed 's/[[:space:]]\+/ /g')"
      out_norm="$(tr -d '
' < "$out" | sed 's/[[:space:]]\+/ /g')"
      status="unchanged"
      if [[ "$in_norm" != "$out_norm" ]]; then
        status="rewritten"
      fi
      printf "%s	%s	%s	%s	%s
" "$expr" "$rules" "$input" "$out" "$status" >> examples/outputs/assessment.tsv
    else
      printf "%s	%s	%s	%s	%s
" "$expr" "$rules" "$input" "$out" "error" >> examples/outputs/assessment.tsv
    fi
  done
done
